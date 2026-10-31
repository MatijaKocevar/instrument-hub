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

constexpr double silenceFloor = 1.0e-12;

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

//==============================================================================
Session::Session()
{
    config.slots = makeDefaultSet();
}

Session::~Session()
{
    closeDevice();
}

std::vector<SessionSlotConfig> Session::makeDefaultSet()
{
    constexpr const char* names[] = {
        "Init 1", "Init 2", "Init 3", "Init 4",
        "Init 5", "Init 6", "Init 7", "Init 8"
    };

    std::vector<SessionSlotConfig> slots;
    slots.reserve (std::size (defaultFrequencies));

    for (std::size_t index = 0; index < std::size (defaultFrequencies); ++index)
    {
        SessionSlotConfig slot;
        slot.name = names[index];
        slot.kind = SessionSlotKind::initTone;
        slot.frequency = defaultFrequencies[index];
        slot.gain = 1.0f;
        slots.push_back (std::move (slot));
    }

    return slots;
}

bool Session::build (const SessionConfig& newConfig, juce::String& error)
{
    error.clear();
    closeDevice();
    prepared = false;

    // Drop the graph before its sources: the slots hold non-owning pointers.
    switcher.reset();
    engine.reset();
    sources.clear();
    instruments.clear();
    kits.clear();
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

    engine = std::make_unique<AudioEngine::AudioEngine> (slotCount,
                                                         config.device.numOutputChannels);
    switcher = std::make_unique<AudioEngine::SlotSwitcher> (config.fadeMilliseconds);

    pluginFormats = std::make_unique<juce::AudioPluginFormatManager>();
    juce::addHeadlessDefaultFormatsToManager (*pluginFormats);

    std::vector<AudioEngine::Slot*> slotPointers;
    slotPointers.reserve (static_cast<std::size_t> (slotCount));

    for (int index = 0; index < slotCount; ++index)
    {
        auto* source = createSource (config.slots[static_cast<std::size_t> (index)],
                                     index,
                                     error);

        if (source == nullptr)
        {
            if (error.isEmpty())
                error = "could not create slot " + juce::String (index);

            switcher.reset();
            engine.reset();
            sources.clear();
            instruments.clear();
            kits.clear();
            return false;
        }

        auto& slot = engine->getSlot (index);
        slot.setSource (source);
        slot.setGain (config.slots[static_cast<std::size_t> (index)].gain);

        sources.push_back (source);
        slotPointers.push_back (&slot);
    }

    // All slots are wired in parallel once. Nothing rewires later.
    switcher->setSlots (slotPointers);

    prepare (config.device.sampleRate,
             config.device.bufferSize,
             config.device.numOutputChannels);

    return true;
}

AudioEngine::SlotSource* Session::createSource (const SessionSlotConfig& slotConfig,
                                                int slotIndex,
                                                juce::String& error)
{
    const auto slotLabel = juce::String (slotIndex) + " (\"" + juce::String (slotConfig.name) + "\")";

    switch (slotConfig.kind)
    {
        case SessionSlotKind::initTone:
        {
            auto instrument = std::make_unique<InitToneSource> (slotConfig.frequency,
                                                                 initToneAmplitude);
            auto* source = static_cast<AudioEngine::SlotSource*> (instrument.get());
            instruments.push_back (std::move (instrument));
            return source;
        }

        case SessionSlotKind::sampler:
        {
            if (slotConfig.path.isEmpty())
            {
                error = "slot " + slotLabel + " is a sampler slot but has no kit path";
                return nullptr;
            }

            auto kit = std::make_unique<sampler::LoadedKit>();

            if (! sampler::KitLoader::loadFromFile (juce::File (slotConfig.path),
                                                    config.device.sampleRate,
                                                    *kit,
                                                    error))
            {
                error = "slot " + slotLabel + ": " + error;
                return nullptr;
            }

            auto instrument = std::make_unique<sampler::SamplerInstrument>();
            instrument->setKit (kit.get());

            auto* source = static_cast<AudioEngine::SlotSource*> (instrument.get());
            instruments.push_back (std::move (instrument));
            kits.push_back (std::move (kit));
            return source;
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

            auto* source = static_cast<AudioEngine::SlotSource*> (instrument.get());
            instruments.push_back (std::move (instrument));
            return source;
        }

        case SessionSlotKind::audioInput:
        {
            auto instrument = std::make_unique<AudioInputInstrument>();
            instrument->setInputChannel (slotConfig.inputChannel);

            auto* source = static_cast<AudioEngine::SlotSource*> (instrument.get());
            instruments.push_back (std::move (instrument));
            return source;
        }
    }

    error = "slot " + slotLabel + " has an unknown source kind";
    return nullptr;
}

//==============================================================================
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

    // The engine owns the slots, the summing bus and the timing monitor. The
    // switcher owns the crossfade envelope over the same slots.
    engine->prepare (sampleRate, maximumBlockSize, numChannels);

    if (switcher != nullptr)
        switcher->prepare (sampleRate, maximumBlockSize, numChannels);

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
void Session::armSlot (int slot) noexcept
{
    if (switcher != nullptr)
        switcher->armSlot (slot);
}

int Session::getArmedSlot() const noexcept
{
    return switcher != nullptr ? switcher->getArmedSlot() : -1;
}

//==============================================================================
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
