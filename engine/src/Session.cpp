#include "Session.h"

#include "audio/RtGuard.h"
#include "instruments/AudioInputInstrument.h"
#include "plugins/PluginInstrument.h"
#include "sampler/KitLoader.h"
#include "sampler/SamplerInstrument.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>

namespace ihub
{

namespace
{
/** Amplitude of the built-in initialisation tone. */
constexpr float initToneAmplitude = 0.12f;

/** Pitch per default slot. Deliberately low: the discontinuity detector's
    continuous-signal baseline is the tone's own curvature, and 110-220 Hz
    leaves more than 10 dB of margin under the -60 dBFS acceptance limit.
*/
constexpr double defaultFrequencies[] = {
    110.0, 123.47, 138.59, 146.83, 164.81, 185.0, 207.65, 220.0
};

/** The eight families of the Launchpad grid, eight patches each. */
constexpr const char* familyIds[] = {
    "keys", "drums", "bass", "pads", "lead", "fx", "perc", "vox"
};

constexpr const char* familyNames[] = {
    "Keys", "Drums", "Bass", "Pads", "Lead", "FX", "Percussion", "Vox"
};

constexpr int patchesPerFamily = 8;

constexpr double silenceFloor = 1.0e-12;

/** Per-block peak-meter decay. At 48 kHz / 128 frames a full-scale peak falls
    by about 20 dB in 26 ms, so a meter frame read at 30 Hz sees a steady value
    while a slot plays instead of the tone's per-block zero crossings.
*/
constexpr float meterDecay = 0.85f;

double toDbfs (double linear) noexcept
{
    return 20.0 * std::log10 (juce::jmax (linear, silenceFloor));
}

/** A phase-continuous sine and a real `Instrument` / `SlotSource`.

    This is the default set's stand-in source until a set with plugins or
    samples is loaded. It demonstrates the composition without any external
    asset and keeps the audio-thread contract: the phase is plain state, the
    render loop never allocates or locks.
*/
class InitToneSource final : public Instrument,
                             public AudioEngine::SlotSource
{
public:
    InitToneSource (double frequencyToUse, float amplitudeToUse)
        : frequency (frequencyToUse > 0.0 ? frequencyToUse : 220.0),
          amplitude (amplitudeToUse)
    {
    }

    void prepare (double newSampleRate, int) override
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
        phase = 0.0;
        increment = juce::MathConstants<double>::twoPi * frequency / sampleRate;
    }

    void noteOn (int ch, int note, float vel) override
    {
        juce::ignoreUnused (ch, note, vel);
    }

    void noteOff (int ch, int note) override
    {
        juce::ignoreUnused (ch, note);
    }

    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        const int samples = buffer.getNumSamples();
        const int channels = buffer.getNumChannels();

        for (int frame = 0; frame < samples; ++frame)
        {
            const auto value = static_cast<float> (amplitude * std::sin (phase));

            for (int channel = 0; channel < channels; ++channel)
                buffer.getWritePointer (channel)[frame] = value;

            phase += increment;

            if (phase >= juce::MathConstants<double>::twoPi)
                phase -= juce::MathConstants<double>::twoPi;
        }
    }

    int latencySamples() const noexcept override { return 0; }

    Kind kind() const noexcept override { return Kind::Sampler; }

private:
    const double frequency;
    const float amplitude;

    double sampleRate = 48000.0;
    double phase = 0.0;
    double increment = 0.0;
};

/** A sampler patch whose kit is owned by the pooled instance.

    The warm pool creates and destroys instruments on its own threads; a kit
    reached through a `Session` member would race with that. Owning it here
    keeps the lifetime identical to the instrument.
*/
class PooledSamplerSource final : public AudioEngine::SlotSource
{
public:
    bool load (const juce::File& file, double sampleRate, juce::String& error)
    {
        kit = std::make_unique<sampler::LoadedKit>();

        if (! sampler::KitLoader::loadFromFile (file, sampleRate, *kit, error))
        {
            kit.reset();
            return false;
        }

        instrument = std::make_unique<sampler::SamplerInstrument>();
        instrument->setKit (kit.get());
        return true;
    }

    void prepare (double sampleRate, int maximumBlockSize) override
    {
        if (instrument != nullptr)
            instrument->prepare (sampleRate, maximumBlockSize);
    }

    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
    {
        if (instrument != nullptr)
            instrument->process (buffer, midi);
        else
            buffer.clear();
    }

    int latencySamples() const noexcept override
    {
        return instrument != nullptr ? instrument->latencySamples() : 0;
    }

private:
    std::unique_ptr<sampler::LoadedKit> kit;
    std::unique_ptr<sampler::SamplerInstrument> instrument;
};

} // namespace

//==============================================================================
const char* toString (SessionSlotKind kind) noexcept
{
    switch (kind)
    {
        case SessionSlotKind::initTone:   return "init-tone";
        case SessionSlotKind::sampler:    return "sampler";
        case SessionSlotKind::plugin:     return "plugin";
        case SessionSlotKind::audioInput: return "audio-input";
    }

    return "init-tone";
}

const char* toString (SessionWarmState state) noexcept
{
    switch (state)
    {
        case SessionWarmState::cold:    return "cold";
        case SessionWarmState::warming: return "warming";
        case SessionWarmState::warm:    return "warm";
        case SessionWarmState::active:  return "active";
        case SessionWarmState::fading:  return "fading";
    }

    return "cold";
}

//==============================================================================
/**
    The permanent source of one fixed engine slot.

    The engine slot never changes its `SlotSource*`, so the graph really is
    fixed. The actual patch instrument lives in the warm pool and is published
    here with an atomic pointer store: arming is one pointer hand-over, not a
    graph edit. While no instance is bound the proxy renders silence.
*/
struct Session::Voice final : public AudioEngine::SlotSource
{
    void prepare (double, int) override {}

    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
    {
        auto* instance = delegate.load (std::memory_order_acquire);

        if (instance == nullptr)
        {
            buffer.clear();
            return;
        }

        instance->process (buffer, midi);
    }

    int latencySamples() const noexcept override
    {
        return latency.load (std::memory_order_relaxed);
    }

    bool acceptsDeviceInput() const noexcept override
    {
        return acceptsInput.load (std::memory_order_relaxed);
    }

    void setInputBlock (const float* const* channelData, int numInputs) noexcept override
    {
        auto* instance = delegate.load (std::memory_order_acquire);

        if (instance != nullptr && acceptsInput.load (std::memory_order_relaxed))
            instance->setInputBlock (channelData, numInputs);
    }

    bool bypassesFxChain() const noexcept override
    {
        return bypassesChain.load (std::memory_order_relaxed);
    }

    void bind (AudioEngine::SlotSource* instance) noexcept
    {
        int latencySamples = 0;
        bool accepts = false;
        bool bypasses = false;

        if (instance != nullptr)
        {
            latencySamples = instance->latencySamples();
            accepts = instance->acceptsDeviceInput();
            bypasses = instance->bypassesFxChain();
        }

        latency.store (latencySamples, std::memory_order_relaxed);
        acceptsInput.store (accepts, std::memory_order_relaxed);
        bypassesChain.store (bypasses, std::memory_order_relaxed);
        delegate.store (instance, std::memory_order_release);
    }

    void clearIf (AudioEngine::SlotSource* instance) noexcept
    {
        if (delegate.load (std::memory_order_acquire) != instance)
            return;

        delegate.store (nullptr, std::memory_order_release);
        latency.store (0, std::memory_order_relaxed);
        acceptsInput.store (false, std::memory_order_relaxed);
        bypassesChain.store (false, std::memory_order_relaxed);
    }

    std::atomic<AudioEngine::SlotSource*> delegate { nullptr };
    std::atomic<int> latency { 0 };
    std::atomic<bool> acceptsInput { false };
    std::atomic<bool> bypassesChain { false };
};

//==============================================================================
Session::Session()
{
    config.slots = makeDefaultSet();
}

Session::~Session()
{
    closeDevice();
    shutdownWarmPool();
    switcher.reset();
    engine.reset();
    voices.clear();
    pluginFormats.reset();
}

std::vector<SessionSlotConfig> Session::makeDefaultSet()
{
    std::vector<SessionSlotConfig> slots;
    slots.reserve (static_cast<std::size_t> (defaultLogicalSlots));

    for (int index = 0; index < defaultLogicalSlots; ++index)
    {
        const int familyIndex = index / patchesPerFamily;
        const int patchIndex = index % patchesPerFamily;
        const double familyScale = std::pow (1.25, static_cast<double> (familyIndex));

        SessionSlotConfig slot;
        slot.name = "Init " + std::to_string (index + 1);
        slot.kind = SessionSlotKind::initTone;
        slot.frequency = defaultFrequencies[patchIndex] * familyScale;
        slot.gain = 1.0f;

        slot.family = familyIds[familyIndex];
        slot.familyName = familyNames[familyIndex];
        slot.patch = "init-" + std::to_string (index);
        slot.patchName = slot.name;

        slots.push_back (std::move (slot));
    }

    return slots;
}

void Session::shutdownWarmPool() noexcept
{
    if (warmPool == nullptr)
        return;

    warmPool->setWarmCallback (nullptr);
    warmPool->clear();
    waitForWarmIdle (2000);
    warmPool.reset();

    pinnedActiveSlot = -1;
    pinnedPreviousSlot = -1;
}

bool Session::build (const SessionConfig& newConfig, juce::String& error)
{
    error.clear();
    closeDevice();
    prepared = false;
    transport.reset();

    // Drop the graph before its proxies: the slots hold non-owning pointers.
    shutdownWarmPool();
    switcher.reset();
    engine.reset();
    voices.clear();
    pluginFormats.reset();

    config = newConfig;

    if (config.slots.empty())
        config.slots = makeDefaultSet();

    if (config.device.sampleRate <= 0.0)
        config.device.sampleRate = 48000.0;

    if (config.device.bufferSize <= 0)
        config.device.bufferSize = 128;

    if (config.device.numOutputChannels <= 0)
        config.device.numOutputChannels = 2;

    const int slotCount = static_cast<int> (config.slots.size());

    if (slotCount <= 0)
    {
        error = "the session description has no slots";
        return false;
    }

    if (slotCount > defaultLogicalSlots)
    {
        error = "the session description has " + juce::String (slotCount)
              + " slots; the Launchpad grid has " + juce::String (defaultLogicalSlots);
        return false;
    }

    engine = std::make_unique<AudioEngine::AudioEngine> (slotCount,
                                                         config.device.numOutputChannels);
    switcher = std::make_unique<AudioEngine::SlotSwitcher> (config.fadeMilliseconds);

    pluginFormats = std::make_unique<juce::AudioPluginFormatManager>();
    juce::addHeadlessDefaultFormatsToManager (*pluginFormats);

    const int neighbours = juce::jmax (1, config.neighbourCount);

    warmPool = std::make_unique<AudioEngine::WarmPool> (neighbours);
    warmPool->setFactory ([this] (const AudioEngine::WarmPool::Request& request,
                                  juce::String& factoryError)
    {
        int slotIndex = -1;

        try
        {
            slotIndex = std::any_cast<int> (request.payload);
        }
        catch (...)
        {
            factoryError = "warm request has no slot payload";
            return std::unique_ptr<AudioEngine::SlotSource>();
        }

        if (config.loadDelayMilliseconds > 0)
            juce::Thread::sleep (config.loadDelayMilliseconds);

        return createPooledSource (slotIndex, factoryError);
    });
    warmPool->setDestroyHook ([this] (AudioEngine::SlotSource& instance)
    {
        clearVoice (&instance);
    });
    warmPool->setWarmCallback ([this] (int slot)
    {
        warmCallback (slot);
    });

    voices.reserve (static_cast<std::size_t> (slotCount));

    std::vector<AudioEngine::Slot*> slotPointers;
    slotPointers.reserve (static_cast<std::size_t> (slotCount));

    for (int index = 0; index < slotCount; ++index)
    {
        auto voice = std::make_unique<Voice>();
        auto* source = static_cast<AudioEngine::SlotSource*> (voice.get());
        voices.push_back (std::move (voice));

        auto& slot = engine->getSlot (index);
        slot.setSource (source);
        slot.setGain (config.slots[static_cast<std::size_t> (index)].gain);

        slotPointers.push_back (&slot);
    }

    // All slots are wired in parallel once. Nothing rewires later; arming only
    // changes which warm-pool instance the fixed proxy points at.
    switcher->setSlots (slotPointers);

    // Give the pool its format before any request can be queued. `build()`
    // calls `setRequest()` before `prepare()`, and a pool without a format
    // would create an unprepared instrument (silent init tone).
    warmPool->setFormat (config.device.sampleRate, config.device.bufferSize);
    warmPool->setNumSlots (slotCount);

    for (int index = 0; index < slotCount; ++index)
    {
        const auto& slotConfig = config.slots[static_cast<std::size_t> (index)];

        AudioEngine::WarmPool::Request request;
        request.id = juce::String (index) + ":" + toString (slotConfig.kind)
                   + ":" + slotConfig.path + ":" + juce::String (slotConfig.frequency);
        request.payload = index;
        warmPool->setRequest (index, std::move (request));
    }

    prepare (config.device.sampleRate,
             config.device.bufferSize,
             config.device.numOutputChannels);

    const int initial = juce::jlimit (0, slotCount - 1, config.initialSlot);
    warmPool->setActiveSlot (initial);
    warmPool->preloadAll();

    requestedSlot.store (-1);
    activeSlot.store (-1);
    pendingArmSlot.store (-1);
    pinnedActiveSlot = -1;
    pinnedPreviousSlot = -1;

    return true;
}

std::unique_ptr<AudioEngine::SlotSource> Session::createPooledSource (int slotIndex,
                                                                      juce::String& error)
{
    if (slotIndex < 0 || slotIndex >= static_cast<int> (config.slots.size()))
    {
        error = "warm request addresses slot " + juce::String (slotIndex);
        return nullptr;
    }

    const auto& slotConfig = config.slots[static_cast<std::size_t> (slotIndex)];
    const auto slotLabel = juce::String (slotIndex) + " (\"" + juce::String (slotConfig.name) + "\")";

    switch (slotConfig.kind)
    {
        case SessionSlotKind::initTone:
            return std::make_unique<InitToneSource> (slotConfig.frequency,
                                                     initToneAmplitude);

        case SessionSlotKind::sampler:
        {
            if (slotConfig.path.isEmpty())
            {
                error = "slot " + slotLabel + " is a sampler slot but has no kit path";
                return nullptr;
            }

            auto samplerSource = std::make_unique<PooledSamplerSource>();

            if (! samplerSource->load (juce::File (slotConfig.path),
                                       config.device.sampleRate,
                                       error))
            {
                error = "slot " + slotLabel + ": " + error;
                return nullptr;
            }

            return samplerSource;
        }

        case SessionSlotKind::plugin:
        {
            if (slotConfig.path.isEmpty())
            {
                error = "slot " + slotLabel + " is a plugin slot but has no bundle path";
                return nullptr;
            }

            auto instrument = std::make_unique<plugins::PluginInstrument>();

            if (! instrument->loadFromFile (*pluginFormats,
                                            juce::File (slotConfig.path),
                                            config.device.sampleRate,
                                            config.device.bufferSize,
                                            error))
            {
                error = "slot " + slotLabel + ": " + error;
                return nullptr;
            }

            return instrument;
        }

        case SessionSlotKind::audioInput:
        {
            auto instrument = std::make_unique<AudioInputInstrument>();
            instrument->setInputChannel (slotConfig.inputChannel);
            return instrument;
        }
    }

    error = "slot " + slotLabel + " has an unknown source kind";
    return nullptr;
}

//==============================================================================
void Session::warmCallback (int slot)
{
    const std::lock_guard<std::mutex> lock (armMutex);

    if (warmPool == nullptr)
        return;

    auto* instance = warmPool->acquire (slot);

    if (instance == nullptr)
        return; // evicted before the hand-off; the next arm retries

    bindVoice (slot, instance);

    if (pendingArmSlot.load (std::memory_order_relaxed) == slot)
    {
        finishArmLocked (slot, instance);
        return;
    }

    warmPool->release (slot);
}

void Session::bindVoice (int slot, AudioEngine::SlotSource* instance) noexcept
{
    if (slot < 0 || slot >= static_cast<int> (voices.size()))
        return;

    voices[static_cast<std::size_t> (slot)]->bind (instance);
}

void Session::clearVoice (AudioEngine::SlotSource* instance) noexcept
{
    if (instance == nullptr)
        return;

    for (auto& voice : voices)
        voice->clearIf (instance);
}

void Session::releaseAllPinsLocked()
{
    if (warmPool == nullptr)
    {
        pinnedActiveSlot = -1;
        pinnedPreviousSlot = -1;
        return;
    }

    const int previous = pinnedPreviousSlot;
    const int active = pinnedActiveSlot;

    pinnedPreviousSlot = -1;
    pinnedActiveSlot = -1;

    if (previous >= 0)
        warmPool->release (previous);

    if (active >= 0)
        warmPool->release (active);
}

bool Session::finishArmLocked (int slot, AudioEngine::SlotSource* pinnedInstance)
{
    if (warmPool == nullptr || switcher == nullptr || pinnedInstance == nullptr)
        return false;

    bindVoice (slot, pinnedInstance);

    // The only audio-thread effect of arming: one lock-free atomic publish.
    switcher->armSlot (slot);

    const int oldActive = pinnedActiveSlot;
    const int oldPrevious = pinnedPreviousSlot;

    activeSlot.store (slot, std::memory_order_relaxed);
    pendingArmSlot.store (-1, std::memory_order_relaxed);

    if (oldActive == slot)
    {
        // Re-arming the same slot: drop the duplicate pin.
        warmPool->release (slot);
        return true;
    }

    if (oldPrevious >= 0 && oldPrevious != slot)
        warmPool->release (oldPrevious);

    pinnedPreviousSlot = oldActive;
    pinnedActiveSlot = slot;

    if (slot == oldPrevious)
        warmPool->release (slot); // it was already pinned as the previous patch

    return true;
}

bool Session::waitForWarmIdle (int timeoutMs)
{
    if (warmPool == nullptr)
        return true;

    const double deadline = juce::Time::getMillisecondCounterHiRes()
                          + static_cast<double> (juce::jmax (0, timeoutMs));

    for (;;)
    {
        if (warmPool->isIdle())
            return true;

        if (juce::Time::getMillisecondCounterHiRes() >= deadline)
            return false;

        // Instance destruction is posted to the message thread, and the pool
        // counts it until the callback runs. Headless offline tools must pump
        // that loop or a retirement would pin the budget forever.
       #if JUCE_MODAL_LOOPS_PERMITTED
        if (auto* manager = juce::MessageManager::getInstanceWithoutCreating())
            manager->runDispatchLoopUntil (2);
        else
            juce::Thread::sleep (2);
       #else
        juce::Thread::sleep (2);
       #endif
    }
}

bool Session::warmSlotBlockingLocked (int slot, int timeoutMs)
{
    if (warmPool == nullptr)
        return false;

    if (warmPool->isWarm (slot))
        return true;

    warmPool->setActiveSlot (slot);
    warmPool->preloadAll();

    // A protected neighbour that cannot fit keeps the pool technically busy
    // (deferred), but the target we were asked for may already be resident.
    (void) waitForWarmIdle (timeoutMs);

    if (warmPool->isWarm (slot))
        return true;

    auto* instance = warmPool->acquire (slot);

    if (instance == nullptr)
        return false;

    bindVoice (slot, instance);
    warmPool->release (slot);
    return true;
}

void Session::prepare (double newSampleRate, int newMaximumBlockSize, int newNumChannels)
{
    if (engine == nullptr)
    {
        prepared = false;
        return;
    }

    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    maximumBlockSize = juce::jmax (1, newMaximumBlockSize);
    numChannels = juce::jmax (1, newNumChannels);

    if (warmPool != nullptr)
        warmPool->setFormat (sampleRate, maximumBlockSize);

    // The engine owns the slots, the summing bus and the timing monitor. The
    // switcher owns the crossfade envelope over the same slots.
    engine->prepare (sampleRate, maximumBlockSize, numChannels);

    if (switcher != nullptr)
        switcher->prepare (sampleRate, maximumBlockSize, numChannels);

    transport.prepare (sampleRate, maximumBlockSize);

    const int slotCount = juce::jmax (1, engine->getNumSlots());

    if (slotMeterPeaks == nullptr || meterSlotCount != slotCount)
    {
        slotMeterPeaks = std::make_unique<std::atomic<float>[]> (
            static_cast<std::size_t> (slotCount));
        meterSlotCount = slotCount;
    }

    for (int slot = 0; slot < meterSlotCount; ++slot)
        slotMeterPeaks[static_cast<std::size_t> (slot)].store (0.0f,
                                                              std::memory_order_relaxed);

    masterMeterPeak.store (0.0f, std::memory_order_relaxed);

    renderBuffer.setSize (numChannels, maximumBlockSize, false, true, true);
    renderBuffer.clear();
    blockMidi.clear();

    prepared = true;
}

void Session::resetGraph() noexcept
{
    if (engine != nullptr)
        engine->reset();

    if (switcher != nullptr)
        switcher->reset();

    transport.rewind();

    for (int slot = 0; slot < meterSlotCount; ++slot)
        slotMeterPeaks[static_cast<std::size_t> (slot)].store (0.0f,
                                                              std::memory_order_relaxed);

    masterMeterPeak.store (0.0f, std::memory_order_relaxed);

    renderBuffer.clear();
    blockMidi.clear();
}

int Session::getNumSlots() const noexcept
{
    return engine != nullptr ? engine->getNumSlots() : 0;
}

AudioEngine::AudioEngine& Session::getEngine() noexcept
{
    jassert (engine != nullptr);
    return *engine;
}

const AudioEngine::AudioEngine& Session::getEngine() const noexcept
{
    jassert (engine != nullptr);
    return *engine;
}

AudioEngine::SlotSwitcher& Session::getSwitcher() noexcept
{
    jassert (switcher != nullptr);
    return *switcher;
}

const AudioEngine::SlotSwitcher& Session::getSwitcher() const noexcept
{
    jassert (switcher != nullptr);
    return *switcher;
}

int Session::getSlotLatencySamples (int slot) const noexcept
{
    if (engine == nullptr || slot < 0 || slot >= engine->getNumSlots())
        return 0;

    return engine->getSlot (slot).latencySamples();
}

float Session::getSlotMeterPeak (int slot) const noexcept
{
    if (slotMeterPeaks == nullptr || slot < 0 || slot >= meterSlotCount)
        return 0.0f;

    return slotMeterPeaks[static_cast<std::size_t> (slot)].load (std::memory_order_relaxed);
}

float Session::getMasterMeterPeak() const noexcept
{
    return masterMeterPeak.load (std::memory_order_relaxed);
}

//==============================================================================
int Session::getNeighbourCount() const noexcept
{
    return config.neighbourCount;
}

int Session::getWarmInstanceLimit() const noexcept
{
    return warmPool != nullptr ? warmPool->getInstanceLimit()
                               : juce::jmin (getLogicalSlotCount(), 1 + config.neighbourCount);
}

int Session::getLiveInstrumentCount() const noexcept
{
    return warmPool != nullptr ? warmPool->getLiveInstanceCount() : 0;
}

int Session::getPeakInstrumentCount() const noexcept
{
    return warmPool != nullptr ? warmPool->getPeakLiveInstanceCount() : 0;
}

bool Session::isSlotWarm (int slot) const noexcept
{
    return warmPool != nullptr && warmPool->isWarm (slot);
}

SessionWarmState Session::getWarmState (int slot) const noexcept
{
    if (slot < 0 || slot >= getNumSlots())
        return SessionWarmState::cold;

    if (pendingArmSlot.load (std::memory_order_relaxed) == slot)
        return SessionWarmState::warming;

    if (activeSlot.load (std::memory_order_relaxed) == slot)
        return SessionWarmState::active;

    if (warmPool != nullptr)
    {
        if (warmPool->isWarming (slot))
            return SessionWarmState::warming;

        if (warmPool->isWarm (slot))
            return SessionWarmState::warm;
    }

    return SessionWarmState::cold;
}

const char* Session::getWarmStateName (int slot) const noexcept
{
    return toString (getWarmState (slot));
}

bool Session::waitForSlotWarm (int slot, int timeoutMs)
{
    const double deadline = juce::Time::getMillisecondCounterHiRes()
                          + static_cast<double> (juce::jmax (0, timeoutMs));

    for (;;)
    {
        if (warmPool != nullptr && warmPool->isWarm (slot))
        {
            const std::lock_guard<std::mutex> lock (armMutex);
            auto* instance = warmPool->acquire (slot);

            if (instance != nullptr)
            {
                bindVoice (slot, instance);
                warmPool->release (slot);
                return true;
            }
        }

        if (juce::Time::getMillisecondCounterHiRes() >= deadline)
            return false;

       #if JUCE_MODAL_LOOPS_PERMITTED
        if (auto* manager = juce::MessageManager::getInstanceWithoutCreating())
            manager->runDispatchLoopUntil (2);
        else
            juce::Thread::sleep (2);
       #else
        juce::Thread::sleep (2);
       #endif
    }
}

bool Session::waitForSlotActive (int slot, int timeoutMs)
{
    const double deadline = juce::Time::getMillisecondCounterHiRes()
                          + static_cast<double> (juce::jmax (0, timeoutMs));

    for (;;)
    {
        if (getWarmState (slot) == SessionWarmState::active)
            return true;

        if (juce::Time::getMillisecondCounterHiRes() >= deadline)
            return false;

       #if JUCE_MODAL_LOOPS_PERMITTED
        if (auto* manager = juce::MessageManager::getInstanceWithoutCreating())
            manager->runDispatchLoopUntil (2);
        else
            juce::Thread::sleep (2);
       #else
        juce::Thread::sleep (2);
       #endif
    }
}

//==============================================================================
bool Session::resolveDevice (AudioEngine::DeviceSetup& setup, juce::String& error)
{
    const auto infos = devices.getDevices();

    const AudioEngine::DeviceInfo* chosen = nullptr;

    for (const auto& info : infos)
    {
        if (! info.hasOutput || info.outputChannelNames.isEmpty())
            continue;

        if (config.device.preferLoopback
            && ! info.name.containsIgnoreCase ("Loopback"))
        {
            continue;
        }

        if (config.device.deviceTypeName.isNotEmpty()
            && info.typeName != config.device.deviceTypeName)
        {
            continue;
        }

        if (config.device.outputDeviceName.isNotEmpty()
            && ! info.name.containsIgnoreCase (config.device.outputDeviceName))
        {
            continue;
        }

        chosen = &info;
        break;
    }

    if (chosen == nullptr)
    {
        if (config.device.preferLoopback)
            error = "no ALSA Loopback output device was enumerated";
        else if (config.device.outputDeviceName.isNotEmpty())
            error = "output device \"" + config.device.outputDeviceName + "\" was not found";
        else
            error = "no audio output device was enumerated";

        return false;
    }

    setup.deviceTypeName = chosen->typeName;
    setup.outputDeviceName = chosen->name;

    const int availableOutputs = chosen->outputChannelNames.size();
    const int requestedOutputs = juce::jmax (1, config.device.numOutputChannels);
    setup.numOutputChannels = juce::jlimit (1, availableOutputs, requestedOutputs);
    setup.outputChannels = AudioEngine::DeviceSetup::channelMask (setup.numOutputChannels);

    setup.inputDeviceName = {};
    setup.numInputChannels = 0;
    setup.inputChannels.clear();

    if (chosen->hasInput
        && ! chosen->inputChannelNames.isEmpty()
        && config.device.numInputChannels > 0)
    {
        setup.inputDeviceName = config.device.inputDeviceName.isNotEmpty()
                                    ? config.device.inputDeviceName
                                    : chosen->name;

        const int availableInputs = chosen->inputChannelNames.size();
        setup.numInputChannels = juce::jlimit (1,
                                               availableInputs,
                                               config.device.numInputChannels);
        setup.inputChannels = AudioEngine::DeviceSetup::channelMask (setup.numInputChannels);
    }

    setup.sampleRate = config.device.sampleRate > 0.0 ? config.device.sampleRate : 48000.0;
    setup.bufferSize = config.device.bufferSize > 0 ? config.device.bufferSize : 128;

    return true;
}

bool Session::openDevice (juce::String* errorMessage)
{
    juce::String localError;
    auto& error = errorMessage != nullptr ? *errorMessage : localError;
    error.clear();

    closeDevice();

    if (engine == nullptr || ! prepared)
    {
        error = "the session graph has not been built";
        return false;
    }

    AudioEngine::DeviceSetup setup;

    if (! resolveDevice (setup, error))
        return false;

    if (! devices.open (setup, &error))
        return false;

    const double actualRate = devices.getCurrentSampleRate();
    const int actualBlock = devices.getCurrentBufferSize();
    const int activeOutputs = devices.getActiveOutputChannels().countNumberOfSetBits();

    numChannels = activeOutputs > 0
                      ? juce::jmin (config.device.numOutputChannels, activeOutputs)
                      : config.device.numOutputChannels;

    prepare (actualRate > 0.0 ? actualRate : config.device.sampleRate,
             actualBlock > 0 ? juce::jmax (actualBlock, config.device.bufferSize)
                             : config.device.bufferSize,
             numChannels);

    deviceOpen = true;
    devices.setAudioCallback (this);
    callbackInstalled = true;
    return true;
}

void Session::closeDevice()
{
    if (callbackInstalled)
    {
        devices.setAudioCallback (nullptr);
        callbackInstalled = false;
    }

    if (devices.isOpen())
        devices.close();

    deviceOpen = false;
}

//==============================================================================
void Session::armSlot (int slot)
{
    if (switcher == nullptr)
        return;

    if (slot < 0)
    {
        const std::lock_guard<std::mutex> lock (armMutex);
        switcher->disarm();
        requestedSlot.store (-1, std::memory_order_relaxed);
        pendingArmSlot.store (-1, std::memory_order_relaxed);
        activeSlot.store (-1, std::memory_order_relaxed);
        return;
    }

    if (slot >= getNumSlots())
        return;

    const std::lock_guard<std::mutex> lock (armMutex);

    // A cold slot reports `warming` from this store until the instance is
    // resident. A warm slot completes below without ever publishing it.
    pendingArmSlot.store (slot, std::memory_order_relaxed);
    requestedSlot.store (slot, std::memory_order_relaxed);

    if (warmPool == nullptr)
        return;

    if (warmPool->isWarm (slot))
    {
        if (auto* instance = warmPool->acquire (slot); instance != nullptr)
        {
            finishArmLocked (slot, instance);
            return;
        }
    }

    // The older previous patch is no longer needed for an instant return.
    // Dropping its pin leaves room for the cold instance inside 1 + K.
    if (pinnedPreviousSlot >= 0 && pinnedPreviousSlot != slot)
    {
        warmPool->release (pinnedPreviousSlot);
        pinnedPreviousSlot = -1;
    }

    // Keep the outgoing patch audible while the new one loads, then let the
    // pool warm the target and its neighbours.
    warmPool->setActiveSlot (slot);
    warmPool->preloadAll();
}

int Session::getArmedSlot() const noexcept
{
    return requestedSlot.load (std::memory_order_relaxed);
}

//==============================================================================
void Session::updateMeters (int numSamples) noexcept
{
    if (slotMeterPeaks == nullptr || engine == nullptr || switcher == nullptr)
        return;

    const int slots = juce::jmin (meterSlotCount, engine->getNumSlots());

    for (int index = 0; index < slots; ++index)
    {
        float peak = 0.0f;

        const auto state = switcher->getSlotState (index);
        const float gain = switcher->getSlotGain (index);

        if (state != AudioEngine::SlotSwitcher::State::idle && gain > 0.0f)
        {
            const auto& rendered = engine->getSlot (index).getOutputBuffer();
            const int channels = rendered.getNumChannels();
            const int frames = juce::jmin (numSamples, rendered.getNumSamples());

            for (int channel = 0; channel < channels; ++channel)
            {
                const auto* samples = rendered.getReadPointer (channel);

                for (int frame = 0; frame < frames; ++frame)
                    peak = juce::jmax (peak, std::abs (samples[frame]));
            }

            peak *= gain;
        }

        auto& meter = slotMeterPeaks[static_cast<std::size_t> (index)];
        const float decayed = meter.load (std::memory_order_relaxed) * meterDecay;
        meter.store (juce::jmax (peak, decayed), std::memory_order_relaxed);
    }

    float masterPeak = 0.0f;
    const int channels = renderBuffer.getNumChannels();
    const int frames = juce::jmin (numSamples, renderBuffer.getNumSamples());

    for (int channel = 0; channel < channels; ++channel)
    {
        const auto* samples = renderBuffer.getReadPointer (channel);

        for (int frame = 0; frame < frames; ++frame)
            masterPeak = juce::jmax (masterPeak, std::abs (samples[frame]));
    }

    const float decayed = masterMeterPeak.load (std::memory_order_relaxed) * meterDecay;
    masterMeterPeak.store (juce::jmax (masterPeak, decayed), std::memory_order_relaxed);
}

void Session::renderInternal (int numSamples, juce::MidiBuffer& midi) noexcept
{
    const int samples = juce::jmin (numSamples, maximumBlockSize);

    if (samples <= 0 || engine == nullptr || switcher == nullptr)
        return;

    auto& bus = engine->getBus();

    bus.beginBlock (samples);

    if (switcher->process (samples, midi))
        bus.add (switcher->getOutputBuffer(), samples);

    bus.finish (renderBuffer, samples);

    // The transport counts rendered frames, so `/transport.positionSamples`
    // advances with the same block sizes the graph actually processed.
    transport.process (samples);

    updateMeters (samples);
}

void Session::render (juce::AudioBuffer<float>& output,
                      juce::MidiBuffer& midi,
                      int numSamples) noexcept
{
    const AudioEngine::RtGuard::Scope rtScope;

    if (! prepared || engine == nullptr || numSamples <= 0)
    {
        output.clear();
        return;
    }

    const int samples = juce::jmin (numSamples, maximumBlockSize);
    renderInternal (samples, midi);

    const int copyChannels = juce::jmin (numChannels, output.getNumChannels());

    for (int channel = 0; channel < copyChannels; ++channel)
        output.copyFrom (channel, 0, renderBuffer, channel, 0, samples);

    for (int channel = copyChannels; channel < output.getNumChannels(); ++channel)
        output.clear (channel, 0, numSamples);

    if (numSamples > samples)
        for (int channel = 0; channel < copyChannels; ++channel)
            output.clear (channel, samples, numSamples - samples);
}

//==============================================================================
SessionSwitchReport Session::measureSwitch (int fromSlot,
                                            int toSlot,
                                            int settleBlocks,
                                            int rmsBlocks,
                                            int switchBlocks)
{
    SessionSwitchReport report;
    report.fromSlot = fromSlot;
    report.toSlot = toSlot;

    if (! prepared || engine == nullptr || switcher == nullptr)
        return report;

    // Re-preparing and re-rendering the graph is only safe with the device
    // stopped. The method is a message-thread / offline tool.
    if (deviceOpen)
        return report;

    const int slotCount = engine->getNumSlots();

    if (fromSlot < 0 || fromSlot >= slotCount
        || toSlot < 0 || toSlot >= slotCount
        || fromSlot == toSlot)
    {
        return report;
    }

    // This is an offline tool: make both endpoints resident and pinned before
    // the references and the real render start from the same state.
    if (warmPool != nullptr)
    {
        const std::lock_guard<std::mutex> lock (armMutex);

        releaseAllPinsLocked();

        // The measurement needs both endpoints resident at once. Temporary
        // neighbours would compete for the 1+K budget with the pinned pair,
        // so shrink the neighbourhood to one (limit 2) for the warm-up and
        // restore it afterwards.
        const int savedNeighbours = config.neighbourCount;
        warmPool->setNeighbourCount (juce::jmin (1, savedNeighbours));

        if (! warmSlotBlockingLocked (fromSlot, 5000))
        {
            warmPool->setNeighbourCount (savedNeighbours);
            return report;
        }

        auto* fromInstance = warmPool->acquire (fromSlot);

        if (fromInstance == nullptr)
        {
            warmPool->setNeighbourCount (savedNeighbours);
            return report;
        }

        if (! warmSlotBlockingLocked (toSlot, 5000))
        {
            warmPool->release (fromSlot);
            warmPool->setNeighbourCount (savedNeighbours);
            return report;
        }

        auto* toInstance = warmPool->acquire (toSlot);

        if (toInstance == nullptr)
        {
            warmPool->release (fromSlot);
            warmPool->setNeighbourCount (savedNeighbours);
            return report;
        }

        bindVoice (fromSlot, fromInstance);
        bindVoice (toSlot, toInstance);

        pinnedActiveSlot = toSlot;
        pinnedPreviousSlot = fromSlot;
        activeSlot.store (toSlot, std::memory_order_relaxed);
        requestedSlot.store (toSlot, std::memory_order_relaxed);
        pendingArmSlot.store (-1, std::memory_order_relaxed);

        warmPool->setNeighbourCount (savedNeighbours);
    }

    const int blockSize = juce::jmax (1, maximumBlockSize);
    const int settleCount = juce::jmax (1, settleBlocks);
    const int rmsCount = juce::jmax (1, rmsBlocks);
    const int switchCount = juce::jmax (1, switchBlocks);

    const int settleSamples = settleCount * blockSize;
    const int rmsSamples = rmsCount * blockSize;
    const int switchSamples = switchCount * blockSize;
    const int totalSamples = settleSamples + rmsSamples + switchSamples;

    // The transition the product promises: a 3 ms equal-power crossfade.
    // The hard-switch control renders with the ramp disabled, so comparing it
    // with this reference is what makes the detector fire.
    const int referenceFadeSamples = juce::jmax (1,
                                                 juce::roundToInt (0.003 * sampleRate));

    auto renderIsolatedSlot = [this, blockSize] (int slotIndex, int numSamples)
    {
        std::vector<float> result;
        result.reserve (static_cast<std::size_t> (numSamples));

        juce::MidiBuffer midi;

        while (static_cast<int> (result.size()) < numSamples)
        {
            {
                const AudioEngine::RtGuard::Scope rtScope;
                engine->getSlot (slotIndex).processUnfaded (blockSize, midi);
            }

            const auto& rendered = engine->getSlot (slotIndex).getOutputBuffer();
            const int frames = juce::jmin (blockSize, numSamples - static_cast<int> (result.size()));

            for (int frame = 0; frame < frames; ++frame)
                result.push_back (rendered.getSample (0, frame));
        }

        return result;
    };

    // Reference source signals from the same prepared starting state as the
    // real render. `from` runs for the whole timeline; `to` starts at the
    // switch, exactly as SlotSwitcher starts it.
    prepare (sampleRate, maximumBlockSize, numChannels);
    auto fromReference = renderIsolatedSlot (fromSlot, totalSamples);

    prepare (sampleRate, maximumBlockSize, numChannels);
    auto toReference = renderIsolatedSlot (toSlot, switchSamples);

    // The real session render.
    prepare (sampleRate, maximumBlockSize, numChannels);
    resetGraph();

    juce::AudioBuffer<float> block (numChannels, blockSize);
    juce::MidiBuffer midi;

    std::vector<float> captured;
    captured.reserve (static_cast<std::size_t> (rmsSamples + switchSamples));

    armSlot (fromSlot);

    // Let the fade-in finish before measuring the armed slot.
    for (int i = 0; i < settleCount; ++i)
        render (block, midi, blockSize);

    double sumSquares = 0.0;
    int measuredSamples = 0;

    for (int i = 0; i < rmsCount; ++i)
    {
        render (block, midi, blockSize);

        for (int frame = 0; frame < blockSize; ++frame)
        {
            const float sample = block.getSample (0, frame);
            sumSquares += static_cast<double> (sample) * static_cast<double> (sample);
            ++measuredSamples;
            captured.push_back (sample);
        }
    }

    if (measuredSamples > 0)
    {
        report.rms = std::sqrt (sumSquares / static_cast<double> (measuredSamples));
        report.rmsDbfs = toDbfs (report.rms);
    }

    armSlot (toSlot);

    for (int i = 0; i < switchCount; ++i)
    {
        render (block, midi, blockSize);

        for (int frame = 0; frame < blockSize; ++frame)
            captured.push_back (block.getSample (0, frame));
    }

    double maxDelta = 0.0;
    double maxResidual = 0.0;

    for (std::size_t index = 1; index < captured.size(); ++index)
        maxDelta = juce::jmax (maxDelta,
                               std::abs (static_cast<double> (captured[index])
                                         - static_cast<double> (captured[index - 1])));

    const int switchInCaptured = rmsSamples;

    for (std::size_t index = 0; index < captured.size(); ++index)
    {
        const int global = settleSamples + static_cast<int> (index);
        double reference = fromReference[static_cast<std::size_t> (global)];

        const int local = static_cast<int> (index) - switchInCaptured;

        if (local >= 0)
        {
            const float phase = juce::jlimit (0.0f,
                                              1.0f,
                                              static_cast<float> (local)
                                                  / static_cast<float> (referenceFadeSamples));

            const double outgoing = std::cos (phase * juce::MathConstants<double>::halfPi);
            const double incoming = std::sin (phase * juce::MathConstants<double>::halfPi);

            const auto toIndex = static_cast<std::size_t> (local);
            const double toValue = toIndex < toReference.size()
                                       ? static_cast<double> (toReference[toIndex])
                                       : 0.0;

            reference = reference * outgoing + toValue * incoming;
        }

        maxResidual = juce::jmax (maxResidual,
                                  std::abs (static_cast<double> (captured[index]) - reference));
    }

    report.maxSampleDelta = maxDelta;
    report.maxSampleDeltaDbfs = toDbfs (maxDelta);
    report.discontinuity = maxResidual;
    report.discontinuityDbfs = toDbfs (maxResidual);
    report.samplesMeasured = static_cast<int> (captured.size());
    report.rendered = true;
    return report;
}

//==============================================================================
void Session::copyToOutput (float* const* outputChannelData,
                            int numOutputChannels,
                            int numSamples) noexcept
{
    const int copyChannels = juce::jmin (numChannels, numOutputChannels);

    for (int channel = 0; channel < copyChannels; ++channel)
        if (outputChannelData[channel] != nullptr)
            juce::FloatVectorOperations::copy (outputChannelData[channel],
                                              renderBuffer.getReadPointer (channel),
                                              numSamples);

    for (int channel = copyChannels; channel < numOutputChannels; ++channel)
        if (outputChannelData[channel] != nullptr)
            juce::FloatVectorOperations::clear (outputChannelData[channel], numSamples);
}

void Session::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    if (device == nullptr || engine == nullptr)
        return;

    const double deviceSampleRate = device->getCurrentSampleRate();
    const int deviceBlockSize = device->getCurrentBufferSizeSamples();

    if (deviceSampleRate <= 0.0 || deviceBlockSize <= 0)
        return;

    const bool needsPrepare = ! prepared
                              || std::abs (deviceSampleRate - sampleRate) > 0.5
                              || deviceBlockSize > maximumBlockSize;

    if (needsPrepare)
    {
        prepare (deviceSampleRate,
                 juce::jmax (deviceBlockSize, maximumBlockSize),
                 numChannels);
    }
    else
    {
        engine->getTimingMonitor().prepare (deviceSampleRate, deviceBlockSize);
    }
}

void Session::audioDeviceStopped()
{
    // Keep the buffers and activation requests: the next start fades in again.
}

void Session::audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                                int numInputChannels,
                                                float* const* outputChannelData,
                                                int numOutputChannels,
                                                int numSamples,
                                                const juce::AudioIODeviceCallbackContext&)
{
    const AudioEngine::RtGuard::Scope rtScope;

    if (engine == nullptr)
    {
        if (outputChannelData != nullptr)
            for (int channel = 0; channel < numOutputChannels; ++channel)
                if (outputChannelData[channel] != nullptr)
                    juce::FloatVectorOperations::clear (outputChannelData[channel], numSamples);

        return;
    }

    const auto timingToken = engine->getTimingMonitor().beginCallback();

    if (outputChannelData == nullptr || ! prepared)
    {
        for (int channel = 0; channel < numOutputChannels; ++channel)
            if (outputChannelData[channel] != nullptr)
                juce::FloatVectorOperations::clear (outputChannelData[channel], numSamples);

        engine->getTimingMonitor().endCallback (timingToken, 0);
        return;
    }

    const int samples = juce::jmin (numSamples, maximumBlockSize);

    // A first-class audio-input slot receives the interface block once per
    // callback, before anything renders.
    for (int slot = 0; slot < engine->getNumSlots(); ++slot)
        engine->getSlot (slot).feedDeviceInput (inputChannelData, numInputChannels);

    blockMidi.clear();
    renderInternal (samples, blockMidi);
    copyToOutput (outputChannelData, numOutputChannels, samples);

    if (numSamples > samples)
        for (int channel = 0; channel < numOutputChannels; ++channel)
            if (outputChannelData[channel] != nullptr)
                juce::FloatVectorOperations::clear (outputChannelData[channel] + samples,
                                                    numSamples - samples);

    engine->getTimingMonitor().endCallback (timingToken, samples);
}

} // namespace ihub
