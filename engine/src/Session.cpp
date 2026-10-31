#include "Session.h"

#include "audio/RtGuard.h"
#include "instruments/AudioInputInstrument.h"
#include "plugins/PluginInstrument.h"
#include "sampler/KitLoader.h"
#include "sampler/SamplerInstrument.h"

// The editor host needs JUCE's GUI modules. Headless offline tools (scanner,
// session smoke, most API tests) link only `juce_audio_processors_headless`,
// so the editor path compiles to a stub that reports a clear error instead of
// dragging a window system into every test binary.
#if defined (JUCE_MODULE_AVAILABLE_juce_gui_basics) && JUCE_MODULE_AVAILABLE_juce_gui_basics
 #include "plugins/PluginWindow.h"
 #define IHUB_SESSION_HAS_EDITORS 1
#else
 #define IHUB_SESSION_HAS_EDITORS 0
#endif

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

//==============================================================================
// Per-patch plugin-state sidecar (T130)
//
// One `slot-<n>.bin` per patch lives next to `set.yaml`. The file is a small
// header, a JSON metadata block and the opaque plugin blob, so the plugin
// chunk itself is never inlined in a JSON document (plan/DECISIONS.md).
// `replaceWithData()` writes a sibling temporary file and renames it, so an
// interrupted save cannot truncate a good patch state.

constexpr juce::uint32 pluginStateFileMagic = 0x49485053u; // "IHPS"
constexpr int pluginStateFileVersion = 1;
constexpr juce::int64 maximumPluginStateFileBytes = 64 * 1024 * 1024;
constexpr int maximumPluginStateMetadataBytes = 64 * 1024;

juce::File pluginStateFileFor (const juce::File& directory, int slot)
{
    return directory.getChildFile ("slot-" + juce::String (slot) + ".bin");
}

bool writePluginStateFile (const juce::File& file,
                           int slot,
                           const juce::MemoryBlock& data,
                           const juce::String& pluginPath,
                           const juce::String& pluginId,
                           const juce::String& pluginName,
                           std::uint64_t revision)
{
    if (data.getSize() == 0
        || static_cast<juce::int64> (data.getSize()) > maximumPluginStateFileBytes)
        return false;

    auto metadata = juce::var (new juce::DynamicObject());
    auto* fields = metadata.getDynamicObject();
    fields->setProperty ("slot", slot);
    fields->setProperty ("revision", static_cast<juce::int64> (revision));
    fields->setProperty ("path", pluginPath);
    fields->setProperty ("pluginId", pluginId);
    fields->setProperty ("pluginName", pluginName);

    const auto json = juce::JSON::toString (metadata, false);
    const auto metadataSize = json.getNumBytesAsUTF8();

    if (metadataSize <= 0 || metadataSize > maximumPluginStateMetadataBytes)
        return false;

    juce::MemoryOutputStream stream;
    stream.writeInt (static_cast<int> (pluginStateFileMagic));
    stream.writeInt (pluginStateFileVersion);
    stream.writeInt (static_cast<int> (metadataSize));
    stream.write (json.toRawUTF8(), metadataSize);
    stream.writeInt64 (static_cast<juce::int64> (data.getSize()));

    if (data.getSize() > 0)
        stream.write (data.getData(), data.getSize());

    if (! file.getParentDirectory().createDirectory().wasOk())
        return false;

    return file.replaceWithData (stream.getData(), stream.getDataSize());
}

struct PluginStateFile
{
    juce::MemoryBlock data;
    juce::String pluginPath;
    juce::String pluginId;
    juce::String pluginName;
    std::uint64_t revision = 0;
};

bool readPluginStateFileData (const juce::File& file, int expectedSlot, PluginStateFile& result)
{
    if (! file.existsAsFile()
        || file.getSize() <= 0
        || file.getSize() > maximumPluginStateFileBytes)
        return false;

    juce::FileInputStream stream (file);

    if (! stream.openedOk())
        return false;

    if (stream.readInt() != static_cast<int> (pluginStateFileMagic)
        || stream.readInt() != pluginStateFileVersion)
        return false;

    const int metadataSize = stream.readInt();

    if (metadataSize <= 0 || metadataSize > maximumPluginStateMetadataBytes)
        return false;

    juce::MemoryBlock metadataBlock;
    metadataBlock.setSize (static_cast<std::size_t> (metadataSize));

    if (stream.read (metadataBlock.getData(), metadataSize) != metadataSize)
        return false;

    const auto metadata = juce::JSON::parse (
        juce::String::fromUTF8 (static_cast<const char*> (metadataBlock.getData()),
                                metadataSize));

    if (! metadata.isObject())
        return false;

    const auto* fields = metadata.getDynamicObject();

    if (fields == nullptr
        || static_cast<int> (fields->getProperty ("slot")) != expectedSlot)
        return false;

    const juce::int64 dataSize = stream.readInt64();

    if (dataSize <= 0 || dataSize > maximumPluginStateFileBytes)
        return false;

    const auto remaining = file.getSize() - stream.getPosition();

    if (remaining < dataSize)
        return false;

    result.data.setSize (static_cast<std::size_t> (dataSize));

    if (stream.read (result.data.getData(), static_cast<int> (dataSize))
        != static_cast<int> (dataSize))
        return false;

    result.pluginPath = fields->getProperty ("path").toString();
    result.pluginId = fields->getProperty ("pluginId").toString();
    result.pluginName = fields->getProperty ("pluginName").toString();
    result.revision = static_cast<std::uint64_t> (
        static_cast<juce::int64> (fields->getProperty ("revision")));

    return true;
}

/** True when a stored blob was written for the plugin now assigned to the
    slot. An empty metadata field is accepted (older sidecar or a descriptor
    without a bundle path), but a conflicting value is not. */
bool pluginStateMatches (const PluginStateFile& stored,
                         const juce::String& pluginPath,
                         const juce::String& pluginId)
{
    if (stored.pluginPath.isNotEmpty() && pluginPath.isNotEmpty()
        && stored.pluginPath != pluginPath)
        return false;

    if (stored.pluginId.isNotEmpty() && pluginId.isNotEmpty()
        && stored.pluginId != pluginId)
        return false;

    return true;
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

/** A real `SlotSource` that renders nothing.

    Unassigned patches use it so the fixed graph and warm pool stay uniform:
    the slot is still warm/active, its meter is zero, and MIDI is ignored.
*/
class SilentSource final : public AudioEngine::SlotSource
{
public:
    void prepare (double, int) override {}

    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        buffer.clear();
    }

    int latencySamples() const noexcept override { return 0; }
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
        case SessionSlotKind::empty:      return "empty";
        case SessionSlotKind::initTone:   return "init-tone";
        case SessionSlotKind::sampler:    return "sampler";
        case SessionSlotKind::plugin:     return "plugin";
        case SessionSlotKind::audioInput: return "audio-input";
    }

    return "empty";
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
#if IHUB_SESSION_HAS_EDITORS
/** Message-thread owner of one native editor per logical slot.

    Instances are registered by the warm-pool factory and forgotten by the
    pool's destroy hook. Destroy is posted to the message thread, so a pointer
    read here cannot be freed while an editor is being opened.

    The window array is message-thread only. `openEditor`/`closeEditor` marshal
    from HTTP or controller threads with `MessageManager::callSync`; the flags
    read by `isEditorOpen()` are atomics so the UI can poll them safely.
*/
struct Session::EditorHost final
{
    explicit EditorHost (Session& ownerToUse) : session (ownerToUse) {}

    bool open (int slot, std::string& error)
    {
        auto* manager = juce::MessageManager::getInstanceWithoutCreating();

        if (manager == nullptr)
        {
            error = "the message loop is not running; cannot create a native editor";
            return false;
        }

        // Assigning a plug-in source queues its instance on the warm pool. A
        // click that arrives before the pool has published the instance waits
        // here (the HTTP thread) instead of racing the loader. The message
        // thread keeps running, so plugin creation itself can complete.
        if (! manager->isThisTheMessageThread() && isPluginSlot (slot))
        {
            const double deadline = juce::Time::getMillisecondCounterHiRes() + 20000.0;

            while (findInstrument (slot) == nullptr
                   && juce::Time::getMillisecondCounterHiRes() < deadline)
                juce::Thread::sleep (5);
        }

        auto result = juce::MessageManager::callSync ([this, slot]
        {
            return openOnMessageThread (slot);
        });

        if (! result.has_value())
        {
            error = "could not reach the message thread";
            return false;
        }

        error = std::move (result->second);
        return result->first;
    }

    bool close (int slot, std::string& error)
    {
        if (slot < 0 || slot >= Session::defaultLogicalSlots)
        {
            error = "patch slot is out of range";
            return false;
        }

        auto* manager = juce::MessageManager::getInstanceWithoutCreating();

        if (manager == nullptr)
        {
            error = "the message loop is not running; cannot close a native editor";
            return false;
        }

        auto result = juce::MessageManager::callSync ([this, slot]
        {
            return closeOnMessageThread (slot);
        });

        if (! result.has_value())
        {
            error = "could not reach the message thread";
            return false;
        }

        error = std::move (result->second);
        return result->first;
    }

    bool isOpen (int slot) const noexcept
    {
        return slot >= 0 && slot < Session::defaultLogicalSlots
            && openFlags[static_cast<std::size_t> (slot)].load (std::memory_order_acquire);
    }

    int openCount() const noexcept
    {
        return openWindowCount.load (std::memory_order_acquire);
    }

    void registerInstrument (int slot, plugins::PluginInstrument* instrument)
    {
        if (instrument == nullptr || slot < 0 || slot >= Session::defaultLogicalSlots)
            return;

        const std::lock_guard<std::mutex> lock (instrumentMutex);
        instruments[static_cast<std::size_t> (slot)] = instrument;
    }

    /** Pool destroy hook, on the message thread, before the processor dies. */
    void closeBeforeDestroy (AudioEngine::SlotSource& instance) noexcept
    {
        auto* instrument = dynamic_cast<plugins::PluginInstrument*> (&instance);

        if (instrument == nullptr)
            return;

        {
            const std::lock_guard<std::mutex> lock (instrumentMutex);

            for (auto& entry : instruments)
                if (entry == instrument)
                    entry = nullptr;
        }

        auto slot = findSlotForProcessor (instrument->getPlugin());

        if (slot >= 0)
            (void) closeOnMessageThread (slot);
    }

    /** Closes every window. Must run on the message thread when possible. */
    void closeAll()
    {
        auto closeHere = [this]
        {
            for (auto& window : windows)
            {
                if (window == nullptr)
                    continue;

                window->onClose = nullptr;
                window->onBeforeClose = nullptr;
                window->close();
                window.reset();
            }

            for (auto& flag : openFlags)
                flag.store (false, std::memory_order_release);

            openWindowCount.store (0, std::memory_order_release);

            const std::lock_guard<std::mutex> lock (instrumentMutex);

            for (auto& entry : instruments)
                entry = nullptr;
        };

        auto* manager = juce::MessageManager::getInstanceWithoutCreating();

        if (manager != nullptr && ! manager->isThisTheMessageThread())
            juce::MessageManager::callSync (closeHere);
        else
            closeHere();
    }

    /** Closes every editor whose slot is not `keepSlot` (pass -1 for all).

        Called when the armed patch changes: an editor left open on the old
        patch would otherwise keep showing (and editing) a patch that is no
        longer armed. Each window's `onBeforeClose` captures the patch state
        first, so closing does not lose an edit.
    */
    void closeEditorsExcept (int keepSlot)
    {
        auto* manager = juce::MessageManager::getInstanceWithoutCreating();

        if (manager == nullptr)
            return;

        auto closeHere = [this, keepSlot]
        {
            for (int slot = 0; slot < static_cast<int> (windows.size()); ++slot)
            {
                if (slot == keepSlot)
                    continue;

                auto& window = windows[static_cast<std::size_t> (slot)];

                if (window != nullptr && window->isOpen())
                    window->close(); // onBeforeClose captures, onClose clears flags
            }
        };

        if (! manager->isThisTheMessageThread())
            juce::MessageManager::callSync (closeHere);
        else
            closeHere();
    }

private:
    bool isPluginSlot (int slot) const
    {
        const std::lock_guard<std::mutex> lock (session.sourceMutex);

        return slot >= 0
            && slot < static_cast<int> (session.sourceConfigs.size())
            && session.sourceConfigs[static_cast<std::size_t> (slot)].kind
                   == SessionSlotKind::plugin;
    }

    bool readSlotConfig (int slot, SessionSlotConfig& config) const
    {
        const std::lock_guard<std::mutex> lock (session.sourceMutex);

        if (slot < 0 || slot >= static_cast<int> (session.sourceConfigs.size()))
            return false;

        config = session.sourceConfigs[static_cast<std::size_t> (slot)];
        return true;
    }

    std::pair<bool, std::string> openOnMessageThread (int slot)
    {
        SessionSlotConfig slotConfig;

        if (! readSlotConfig (slot, slotConfig))
            return { false, "patch slot is out of range" };

        const auto label = "slot " + std::to_string (slot);

        if (slotConfig.kind != SessionSlotKind::plugin)
            return { false, label + " is a " + toString (slotConfig.kind)
                                + " source; only plugin patches have a native editor" };

        auto* instrument = findInstrument (slot);

        if (instrument == nullptr)
            return { false, "the plugin for " + label
                                + " is not loaded; arm the patch and wait for it to warm" };

        auto* processor = instrument->getPlugin();

        if (processor == nullptr)
            return { false, "the plugin for " + label + " has no live instance" };

        auto& window = windows[static_cast<std::size_t> (slot)];

        if (window == nullptr)
        {
            window = std::make_unique<plugins::PluginWindow>();

            // Capture the patch state while the editor and processor are still
            // alive. Closing an editor is a natural save point (T130).
            window->onBeforeClose = [this, slot]
            {
                (void) session.capturePluginState (slot);
            };

            window->onClose = [this, slot]
            {
                const auto index = static_cast<std::size_t> (slot);
                openFlags[index].store (false, std::memory_order_release);
                openWindowCount.fetch_sub (1, std::memory_order_acq_rel);
            };
        }
        else if (window->isOpen())
        {
            if (window->getProcessor() == processor)
            {
                if (auto* component = window->getWindowComponent())
                    component->toFront (true);

                return { true, {} };
            }

            // The slot was rebound to a different instance while this window
            // was still up. Close it before creating the new editor.
            (void) closeOnMessageThread (slot);
        }

        juce::String windowError;
        const juce::String title = slotConfig.pluginName.empty()
                                       ? juce::String (slotConfig.name)
                                       : juce::String (slotConfig.pluginName);

        if (! window->open (*instrument, title, nullptr, &windowError))
        {
            auto message = windowError.isNotEmpty()
                               ? windowError.toStdString()
                               : std::string ("the plugin did not create an editor");

            return { false, label + ": " + message };
        }

        openFlags[static_cast<std::size_t> (slot)].store (true, std::memory_order_release);
        openWindowCount.fetch_add (1, std::memory_order_acq_rel);

        return { true, {} };
    }

    std::pair<bool, std::string> closeOnMessageThread (int slot)
    {
        if (slot < 0 || slot >= static_cast<int> (windows.size()))
            return { false, "patch slot is out of range" };

        auto& window = windows[static_cast<std::size_t> (slot)];

        if (window != nullptr && window->isOpen())
            window->close(); // onClose clears the flag and drops the count

        return { true, {} };
    }

    plugins::PluginInstrument* findInstrument (int slot) const
    {
        if (slot < 0 || slot >= Session::defaultLogicalSlots)
            return nullptr;

        const std::lock_guard<std::mutex> lock (instrumentMutex);
        return instruments[static_cast<std::size_t> (slot)];
    }

    int findSlotForProcessor (juce::AudioProcessor* processor) const
    {
        if (processor == nullptr)
            return -1;

        for (int slot = 0; slot < static_cast<int> (windows.size()); ++slot)
            if (windows[static_cast<std::size_t> (slot)] != nullptr
                && windows[static_cast<std::size_t> (slot)]->isOpen()
                && windows[static_cast<std::size_t> (slot)]->getProcessor() == processor)
                return slot;

        return -1;
    }

    Session& session;

    mutable std::mutex instrumentMutex;
    std::array<plugins::PluginInstrument*, Session::defaultLogicalSlots> instruments {};

    // Message-thread only.
    std::array<std::unique_ptr<plugins::PluginWindow>,
               Session::defaultLogicalSlots> windows {};

    std::array<std::atomic<bool>, Session::defaultLogicalSlots> openFlags {};
    std::atomic<int> openWindowCount { 0 };
};
#else
/** Headless builds have no window system: report the limitation clearly. */
struct Session::EditorHost final
{
    explicit EditorHost (Session&) {}

    bool open (int slot, std::string& error)
    {
        if (slot < 0 || slot >= Session::defaultLogicalSlots)
        {
            error = "patch slot is out of range";
            return false;
        }

        error = "native plugin editors are not available in this build";
        return false;
    }

    bool close (int slot, std::string& error)
    {
        if (slot < 0 || slot >= Session::defaultLogicalSlots)
        {
            error = "patch slot is out of range";
            return false;
        }

        error.clear();
        return true;
    }

    bool isOpen (int) const noexcept { return false; }
    int openCount() const noexcept { return 0; }
    void registerInstrument (int, plugins::PluginInstrument*) {}
    void closeBeforeDestroy (AudioEngine::SlotSource&) noexcept {}
    void closeEditorsExcept (int) {}
    void closeAll() {}
};
#endif

//==============================================================================
Session::Session()
{
    config.slots = makeDefaultSet();
    pluginStates.resize (defaultLogicalSlots);
    editorHost = std::make_unique<EditorHost> (*this);
}

Session::~Session()
{
    closeDevice();

    // The editors reference processors owned by the warm pool, so close them
    // before any instance is destroyed and before the pool is torn down.
    if (editorHost != nullptr)
        editorHost->closeAll();

    shutdownWarmPool();
    switcher.reset();
    engine.reset();
    fxRacks.clear();
    voices.clear();
    pluginFormats.reset();
    editorHost.reset();
}

std::vector<SessionSlotConfig> Session::makeDefaultSet (SessionSlotKind defaultKind)
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
        slot.kind = defaultKind;
        slot.instrumentKind = toString (defaultKind);
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

    // Editors reference the old instances; close them before the graph and the
    // warm pool are dropped.
    if (editorHost != nullptr)
        editorHost->closeAll();

    // Drop the graph before its proxies: the slots hold non-owning pointers.
    shutdownWarmPool();
    switcher.reset();
    engine.reset();
    fxRacks.clear();
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

    {
        // The pool factory reads this copy, so a later source swap can never
        // race a load that is already in flight.
        const std::lock_guard<std::mutex> lock (sourceMutex);
        sourceConfigs = config.slots;
        sourceRevision = 0;
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

   #if IHUB_SESSION_HAS_EDITORS
    // The GUI build hosts the standard VST3 format, whose instances expose
    // their native editor. The headless format would return hasEditor() ==
    // false even for a plugin that has a window.
    juce::addDefaultFormatsToManager (*pluginFormats);
   #else
    juce::addHeadlessDefaultFormatsToManager (*pluginFormats);
   #endif

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
        // Capture before the processor is destroyed. The pool may evict a
        // patch at any time, so this is the last reliable point to store its
        // state (T130). The editor host then closes the window while the
        // processor is still alive.
        auto* instrument = dynamic_cast<plugins::PluginInstrument*> (&instance);

        if (instrument != nullptr)
        {
            capturePluginStateForInstance (instance);
            unregisterPluginInstrument (*instrument);
        }

        if (editorHost != nullptr)
            editorHost->closeBeforeDestroy (instance);

        clearVoice (&instance);
    });
    warmPool->setWarmCallback ([this] (int slot)
    {
        warmCallback (slot);
    });

    voices.reserve (static_cast<std::size_t> (slotCount));

    bool anySolo = false;

    for (const auto& slotConfig : config.slots)
        anySolo = anySolo || slotConfig.solo;

    std::vector<AudioEngine::Slot*> slotPointers;
    slotPointers.reserve (static_cast<std::size_t> (slotCount));

    for (int index = 0; index < slotCount; ++index)
    {
        auto voice = std::make_unique<Voice>();
        auto* source = static_cast<AudioEngine::SlotSource*> (voice.get());
        voices.push_back (std::move (voice));

        const auto& slotConfig = config.slots[static_cast<std::size_t> (index)];
        const bool silent = slotConfig.muted || (anySolo && ! slotConfig.solo);

        auto& slot = engine->getSlot (index);
        slot.setSource (source);
        slot.setGain (silent ? 0.0f : slotConfig.gain);

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

    // The rack owns the insert processors; the slot's FxChain only holds
    // non-owning pointers, so build the racks after the graph is prepared and
    // rebuild each chain once from the persisted set.
    fxRacks.clear();
    fxRacks.reserve (static_cast<std::size_t> (slotCount));

    for (int index = 0; index < slotCount; ++index)
    {
        auto rack = std::make_unique<AudioEngine::FxRack> (pluginFormats.get());
        const auto& slotConfig = config.slots[static_cast<std::size_t> (index)];

        for (const auto& fx : slotConfig.fxChain)
        {
            std::string fxError;

            if (! rack->add (fx, -1, sampleRate, maximumBlockSize, numChannels, fxError))
                juce::Logger::writeToLog ("session: slot " + juce::String (index)
                                          + " insert skipped: " + juce::String (fxError));
        }

        rack->setTransport (currentFxTransport());
        rack->rebuild (engine->getSlot (index).getFxChain(),
                       sampleRate, maximumBlockSize, numChannels);
        fxRacks.push_back (std::move (rack));
    }

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
    SessionSlotConfig slotConfig;

    {
        const std::lock_guard<std::mutex> lock (sourceMutex);

        if (slotIndex < 0 || slotIndex >= static_cast<int> (sourceConfigs.size()))
        {
            error = "warm request addresses slot " + juce::String (slotIndex);
            return nullptr;
        }

        slotConfig = sourceConfigs[static_cast<std::size_t> (slotIndex)];
    }

    const auto slotLabel = juce::String (slotIndex) + " (\"" + juce::String (slotConfig.name) + "\")";

    switch (slotConfig.kind)
    {
        case SessionSlotKind::empty:
            return std::make_unique<SilentSource>();

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

            // Restore this patch's own state before the instance is published
            // to a slot. The warm pool prepares it afterwards, exactly as if a
            // fresh preset had been chosen in the plugin.
            applyStoredPluginState (slotIndex,
                                    *instrument,
                                    juce::String (slotConfig.path),
                                    juce::String (slotConfig.pluginId),
                                    juce::String (slotConfig.pluginName),
                                    true); // a brand-new instance

            // The editor host resolves this slot to this instance. An older
            // instance stays registered until its own destroy hook forgets it.
            if (editorHost != nullptr)
                editorHost->registerInstrument (slotIndex, instrument.get());

            registerPluginInstrument (slotIndex, *instrument);

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

    // A patch that warmed in the background is restored from its own blob
    // before it becomes audible. A fresh instance already got the state in
    // the pool factory; a resident one whose revision moved on is brought up
    // to date here.
    applyStoredPluginState (slot);

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

    // Pre-size the block MIDI storage so draining the queue on the audio
    // thread never grows the underlying array. Each event costs a 6-byte
    // header (int32 sample + uint16 size) plus its 3-byte payload, so 12
    // bytes per queued event is a safe bound.
    blockMidi.ensureSize (static_cast<std::size_t> (midiQueueCapacity) * 12u + 64u);

    // A device change is a clean hand-off point: drop events queued for the
    // stopped stream instead of replaying them into a later block.
    midiFifo.reset();

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

//==============================================================================
// Per-patch insert chain (T121)

AudioEngine::FxTransport Session::currentFxTransport() const noexcept
{
    const auto snapshot = transport.snapshot();

    AudioEngine::FxTransport result;
    result.playing = snapshot.playing;
    result.bpm = snapshot.bpm;
    result.ppq = snapshot.ppq;
    return result;
}

bool Session::beginFxEditLocked (int slot, std::string& error)
{
    if (! prepared || engine == nullptr || slot < 0
        || slot >= static_cast<int> (fxRacks.size()))
    {
        error = "patch slot is out of range";
        return false;
    }

    auto& audioSlot = engine->getSlot (slot);
    audioSlot.setFxChainEditing (true);

    // A running device callback gets one block to see the gate, so the chain
    // vector cannot be read while the rack rebuilds it. Offline sessions have
    // no concurrent audio path and edit immediately.
    if (deviceOpen && callbackInstalled)
    {
        const auto before = renderEpoch.load (std::memory_order_acquire);
        const double deadline = juce::Time::getMillisecondCounterHiRes() + 250.0;

        while (renderEpoch.load (std::memory_order_acquire) == before)
        {
            if (juce::Time::getMillisecondCounterHiRes() >= deadline)
            {
                audioSlot.setFxChainEditing (false);
                error = "the audio thread did not reach a block boundary; try again";
                return false;
            }

            juce::Thread::sleep (1);
        }
    }

    return true;
}

void Session::endFxEditLocked (int slot) noexcept
{
    if (engine != nullptr && slot >= 0 && slot < engine->getNumSlots())
        engine->getSlot (slot).setFxChainEditing (false);
}

void Session::rebuildFxChainLocked (int slot)
{
    if (engine == nullptr || slot < 0 || slot >= static_cast<int> (fxRacks.size()))
        return;

    auto& audioSlot = engine->getSlot (slot);
    fxRacks[static_cast<std::size_t> (slot)]->rebuild (audioSlot.getFxChain(),
                                                       sampleRate,
                                                       maximumBlockSize,
                                                       numChannels);
    audioSlot.getFxChain().refreshLatency();
}

bool Session::makeFxSnapshotLocked (int slot,
                                    AudioEngine::FxChainSnapshot& snapshot) const
{
    if (engine == nullptr || slot < 0 || slot >= static_cast<int> (fxRacks.size()))
        return false;

    const auto& audioSlot = engine->getSlot (slot);
    const auto& rack = *fxRacks[static_cast<std::size_t> (slot)];

    const int total = audioSlot.latencySamples();
    const int fxLatency = rack.latencySamples();

    snapshot = {};
    snapshot.slot = slot;
    snapshot.fxLatencySamples = fxLatency;
    snapshot.latencySamples = total;
    snapshot.instrumentLatencySamples = juce::jmax (0, total - fxLatency);
    snapshot.latencyMs = sampleRate > 0.0
                             ? static_cast<double> (total) * 1000.0 / sampleRate
                             : 0.0;
    snapshot.inserts = rack.snapshots();
    return true;
}

bool Session::setSlotSource (int slot,
                             const SessionSlotConfig& source,
                             std::string& error)
{
    if (slot < 0 || slot >= static_cast<int> (config.slots.size()))
    {
        error = "no audio session is attached";
        return false;
    }

    // Save the outgoing patch before its instance is retired. This covers the
    // Instrument page's source picker and any API source change.
    (void) capturePluginState (slot);

    const auto& previousSource = config.slots[static_cast<std::size_t> (slot)];
    const bool keepsPluginState =
        previousSource.kind == SessionSlotKind::plugin
        && source.kind == SessionSlotKind::plugin
        && previousSource.path == source.path
        && juce::String (previousSource.pluginId) == juce::String (source.pluginId);

    // A source swap retires the hosted processor: close its editor first,
    // while the instance is still alive, instead of waiting for the unload.
    if (editorHost != nullptr)
    {
        std::string closeError;
        (void) editorHost->close (slot, closeError);
    }

    const std::lock_guard<std::mutex> lock (armMutex);

    if (warmPool == nullptr || switcher == nullptr
        || slot < 0 || slot >= static_cast<int> (config.slots.size()))
    {
        error = "no audio session is attached";
        return false;
    }

    if (! keepsPluginState)
        clearPluginState (slot);

    // Keep the patch's identity, name, mix and insert chain: only the source
    // fields are taken from the request.
    auto& current = config.slots[static_cast<std::size_t> (slot)];
    current.kind = source.kind;
    current.path = source.path;
    current.frequency = source.frequency >= 20.0 ? source.frequency
                                                 : current.frequency;
    current.inputChannel = source.inputChannel;
    current.instrumentKind = source.instrumentKind.empty()
                                 ? toString (source.kind)
                                 : source.instrumentKind;
    current.pluginFormat = source.pluginFormat;
    current.pluginId = source.pluginId;
    current.pluginName = source.pluginName;
    current.pluginManufacturer = source.pluginManufacturer;
    current.pluginVersion = source.pluginVersion;
    current.samplePath = source.samplePath;

    {
        const std::lock_guard<std::mutex> sourceLock (sourceMutex);

        if (slot < static_cast<int> (sourceConfigs.size()))
            sourceConfigs[static_cast<std::size_t> (slot)] = current;
    }

    const bool wasActive = activeSlot.load (std::memory_order_relaxed) == slot;
    const bool wasRequested = requestedSlot.load (std::memory_order_relaxed) == slot;

    // Drop our pins before the request retires the old instance. The pool's
    // destroy hook clears the fixed voice's pointer, so the audio thread never
    // reads a deleted source.
    if (pinnedActiveSlot == slot)
        pinnedActiveSlot = -1;

    if (pinnedPreviousSlot == slot)
        pinnedPreviousSlot = -1;

    warmPool->release (slot);

    AudioEngine::WarmPool::Request request;
    request.id = juce::String (slot) + ":" + toString (current.kind) + ":"
               + current.path + ":" + juce::String (current.frequency) + ":"
               + juce::String (++sourceRevision);
    request.payload = slot;

    // A changed id retires the warm instance and queues a fresh one when the
    // slot is protected (the armed slot or a neighbour).
    warmPool->setRequest (slot, std::move (request));

    if (wasActive)
    {
        // Re-bind and re-arm through the normal warm hand-off once the new
        // instance is resident.
        pendingArmSlot.store (slot, std::memory_order_relaxed);
        requestedSlot.store (slot, std::memory_order_relaxed);
    }
    else if (wasRequested)
    {
        // Still warming: the pending hand-off will bind the new instance.
        pendingArmSlot.store (slot, std::memory_order_relaxed);
    }

    SourceAssignedHook hook;

    {
        const std::lock_guard<std::mutex> hookLock (sourceAssignedMutex);
        hook = sourceAssignedHook;
    }

    if (hook)
        hook (slot, config.slots[static_cast<std::size_t> (slot)]);

    return true;
}

void Session::setSourceAssignedHook (SourceAssignedHook hook)
{
    const std::lock_guard<std::mutex> lock (sourceAssignedMutex);
    sourceAssignedHook = std::move (hook);
}

//==============================================================================
// Native plugin editor (T129)

bool Session::openEditor (int slot, std::string& error)
{
    error.clear();

    if (slot < 0 || slot >= static_cast<int> (config.slots.size()))
    {
        error = "patch slot is out of range";
        return false;
    }

    if (editorHost == nullptr)
    {
        error = "no audio session is attached";
        return false;
    }

    return editorHost->open (slot, error);
}

bool Session::closeEditor (int slot, std::string& error)
{
    error.clear();

    if (slot < 0 || slot >= static_cast<int> (config.slots.size()))
    {
        error = "patch slot is out of range";
        return false;
    }

    if (editorHost == nullptr)
    {
        error = "no audio session is attached";
        return false;
    }

    return editorHost->close (slot, error);
}

bool Session::isEditorOpen (int slot) const noexcept
{
    return editorHost != nullptr && editorHost->isOpen (slot);
}

int Session::getOpenEditorCount() const noexcept
{
    return editorHost != nullptr ? editorHost->openCount() : 0;
}

//==============================================================================
// Per-patch plugin state (T130)

void Session::registerPluginInstrument (int slot,
                                       plugins::PluginInstrument& instrument) noexcept
{
    if (slot < 0 || slot >= defaultLogicalSlots)
        return;

    const std::lock_guard<std::mutex> lock (pluginStateMutex);
    pluginInstruments[static_cast<std::size_t> (slot)] = &instrument;
}

void Session::unregisterPluginInstrument (plugins::PluginInstrument& instrument) noexcept
{
    const std::lock_guard<std::mutex> lock (pluginStateMutex);

    for (auto& entry : pluginInstruments)
        if (entry == &instrument)
            entry = nullptr;
}

void Session::capturePluginStateForInstance (AudioEngine::SlotSource& instance)
{
    auto* instrument = dynamic_cast<plugins::PluginInstrument*> (&instance);

    if (instrument == nullptr)
        return;

    int slot = -1;

    {
        const std::lock_guard<std::mutex> lock (pluginStateMutex);

        for (int index = 0; index < static_cast<int> (pluginInstruments.size()); ++index)
        {
            if (pluginInstruments[static_cast<std::size_t> (index)] == instrument)
            {
                slot = index;
                break;
            }
        }
    }

    if (slot >= 0)
        (void) capturePluginState (slot);
}

bool Session::capturePluginState (int slot)
{
    if (slot < 0 || slot >= defaultLogicalSlots)
        return false;

    // Read the source descriptor without holding pluginStateMutex, so the
    // lock order is always sourceMutex -> pluginStateMutex (the factory path).
    juce::String pluginPath;
    juce::String pluginId;
    juce::String pluginName;

    {
        const std::lock_guard<std::mutex> sourceLock (sourceMutex);

        if (slot >= static_cast<int> (sourceConfigs.size())
            || sourceConfigs[static_cast<std::size_t> (slot)].kind
                   != SessionSlotKind::plugin)
            return false;

        const auto& source = sourceConfigs[static_cast<std::size_t> (slot)];
        pluginPath = source.path;
        pluginId = juce::String (source.pluginId);
        pluginName = juce::String (source.pluginName);
    }

    bool changed = false;
    bool captured = false;
    juce::File directory;

    {
        const std::lock_guard<std::mutex> lock (pluginStateMutex);

        directory = pluginStateDirectory;

        if (slot >= static_cast<int> (pluginStates.size())
            || slot >= static_cast<int> (pluginInstruments.size())
            || pluginInstruments[static_cast<std::size_t> (slot)] == nullptr)
            return false;

        auto& state = pluginStates[static_cast<std::size_t> (slot)];
        auto data = pluginInstruments[static_cast<std::size_t> (slot)]
                        ->getStateInformation();

        if (data.getSize() > 0)
        {
            captured = true;

            if (state.data != data)
            {
                state.data = std::move (data);
                ++state.revision;
                changed = true;
            }

            state.pluginPath = pluginPath;
            state.pluginId = pluginId;
            state.pluginName = pluginName;

            // The resident instance now holds this revision.
            state.appliedRevision = state.revision;
        }
    }

    if (! captured)
        return false;

    if (changed && directory != juce::File())
        return writePluginState (slot);

    return true;
}

void Session::applyStoredPluginState (int slot,
                                      plugins::PluginInstrument& instrument,
                                      const juce::String& pluginPath,
                                      const juce::String& pluginId,
                                      const juce::String& pluginName,
                                      bool force)
{
    juce::MemoryBlock data;
    std::uint64_t revision = 0;

    {
        const std::lock_guard<std::mutex> lock (pluginStateMutex);

        if (slot < 0 || slot >= static_cast<int> (pluginStates.size()))
            return;

        auto& state = pluginStates[static_cast<std::size_t> (slot)];

        if (state.data.getSize() == 0)
            return;

        // A blob belongs to the plugin it was captured from. Never hand an old
        // patch's state to a different plug-in assigned to the same slot.
        PluginStateFile stored;
        stored.pluginPath = state.pluginPath;
        stored.pluginId = state.pluginId;

        if (! pluginStateMatches (stored, pluginPath, pluginId))
        {
            state.appliedRevision = state.revision;
            return;
        }

        // `appliedRevision` describes the resident instance. A fresh instance
        // from the warm-pool factory must be restored even when the slot
        // revision has not moved: after an eviction the old instance is gone,
        // so its applied marker says nothing about the replacement.
        if (! force && state.appliedRevision == state.revision
            && state.appliedRevision != 0)
            return;

        data = state.data;
        revision = state.revision;
    }

    // VST3 state restoration takes the message-manager lock on some formats,
    // so never hold `pluginStateMutex` across it: the message thread may be
    // waiting for that same mutex (for example in getPluginInstrument()) and
    // could not pump the lock the plugin needs.
    const bool accepted = instrument.setStateInformation (data);

    if (! accepted)
    {
        juce::Logger::writeToLog ("session: slot " + juce::String (slot)
                                  + " rejected stored plugin state \""
                                  + (pluginName.isEmpty() ? juce::String ("plugin")
                                                          : pluginName)
                                  + "\"");
    }

    // Mark it applied even after a rejection: retrying a blob the plugin
    // refuses on every warm-up would only delay arming. If a capture changed
    // the revision while the plugin was restoring, leave the newer blob for
    // the next arm instead of claiming it was applied.
    const std::lock_guard<std::mutex> lock (pluginStateMutex);

    if (slot >= 0 && slot < static_cast<int> (pluginStates.size())
        && pluginStates[static_cast<std::size_t> (slot)].revision == revision)
        pluginStates[static_cast<std::size_t> (slot)].appliedRevision = revision;
}

void Session::applyStoredPluginState (int slot) noexcept
{
    juce::String pluginPath;
    juce::String pluginId;
    juce::String pluginName;

    {
        const std::lock_guard<std::mutex> sourceLock (sourceMutex);

        if (slot < 0 || slot >= static_cast<int> (sourceConfigs.size())
            || sourceConfigs[static_cast<std::size_t> (slot)].kind
                   != SessionSlotKind::plugin)
            return;

        const auto& source = sourceConfigs[static_cast<std::size_t> (slot)];
        pluginPath = source.path;
        pluginId = juce::String (source.pluginId);
        pluginName = juce::String (source.pluginName);
    }

    plugins::PluginInstrument* instrument = nullptr;

    {
        const std::lock_guard<std::mutex> lock (pluginStateMutex);

        if (slot < 0 || slot >= static_cast<int> (pluginInstruments.size()))
            return;

        instrument = pluginInstruments[static_cast<std::size_t> (slot)];
    }

    if (instrument != nullptr)
        applyStoredPluginState (slot, *instrument, pluginPath, pluginId, pluginName);
}

void Session::clearPluginState (int slot)
{
    if (slot < 0 || slot >= defaultLogicalSlots)
        return;

    juce::File directory;

    {
        const std::lock_guard<std::mutex> lock (pluginStateMutex);

        directory = pluginStateDirectory;

        if (slot >= static_cast<int> (pluginStates.size()))
            return;

        auto& state = pluginStates[static_cast<std::size_t> (slot)];
        state = {};
    }

    if (directory != juce::File())
        pluginStateFileFor (directory, slot).deleteFile();
}

bool Session::writePluginState (int slot)
{
    juce::MemoryBlock data;
    juce::String pluginPath;
    juce::String pluginId;
    juce::String pluginName;
    std::uint64_t revision = 0;
    juce::File directory;

    {
        const std::lock_guard<std::mutex> lock (pluginStateMutex);

        directory = pluginStateDirectory;

        if (slot < 0 || slot >= static_cast<int> (pluginStates.size()))
            return false;

        const auto& state = pluginStates[static_cast<std::size_t> (slot)];

        if (state.data.getSize() == 0)
            return false;

        data = state.data;
        pluginPath = state.pluginPath;
        pluginId = state.pluginId;
        pluginName = state.pluginName;
        revision = state.revision;
    }

    if (directory == juce::File())
        return true; // in-memory only

    return writePluginStateFile (pluginStateFileFor (directory, slot),
                                 slot,
                                 data,
                                 pluginPath,
                                 pluginId,
                                 pluginName,
                                 revision);
}

bool Session::readPluginStateFile (int slot, const juce::File& file)
{
    PluginStateFile stored;

    if (! readPluginStateFileData (file, slot, stored))
        return false;

    auto& state = pluginStates[static_cast<std::size_t> (slot)];
    state.data = std::move (stored.data);
    state.revision = stored.revision;
    state.appliedRevision = 0;
    state.pluginPath = stored.pluginPath;
    state.pluginId = stored.pluginId;
    state.pluginName = stored.pluginName;
    return true;
}

void Session::setPluginStateDirectory (const juce::File& directory)
{
    {
        const std::lock_guard<std::mutex> lock (pluginStateMutex);
        pluginStateDirectory = directory;
    }

    (void) loadPluginStates();
}

bool Session::loadPluginStates()
{
    const std::lock_guard<std::mutex> lock (pluginStateMutex);

    for (auto& state : pluginStates)
        state = {};

    if (pluginStateDirectory == juce::File())
        return false;

    if (! pluginStateDirectory.isDirectory())
        return true; // nothing captured yet

    bool allRead = true;

    for (int slot = 0; slot < static_cast<int> (pluginStates.size()); ++slot)
    {
        const auto file = pluginStateFileFor (pluginStateDirectory, slot);

        if (! file.existsAsFile())
            continue;

        if (! readPluginStateFile (slot, file))
        {
            allRead = false;
            juce::Logger::writeToLog ("session: unreadable plugin state "
                                      + file.getFullPathName());
        }
    }

    return allRead;
}

bool Session::savePluginStates()
{
    juce::File directory;

    {
        const std::lock_guard<std::mutex> lock (pluginStateMutex);
        directory = pluginStateDirectory;
    }

    if (directory == juce::File())
        return false;

    bool saved = true;

    for (int slot = 0; slot < static_cast<int> (pluginStates.size()); ++slot)
    {
        bool hasData = false;

        {
            const std::lock_guard<std::mutex> lock (pluginStateMutex);
            hasData = pluginStates[static_cast<std::size_t> (slot)].data.getSize() > 0;
        }

        if (hasData && ! writePluginState (slot))
            saved = false;
    }

    return saved;
}

bool Session::hasPluginState (int slot) const noexcept
{
    if (slot < 0 || slot >= static_cast<int> (pluginStates.size()))
        return false;

    const std::lock_guard<std::mutex> lock (pluginStateMutex);
    return pluginStates[static_cast<std::size_t> (slot)].data.getSize() > 0;
}

juce::int64 Session::getPluginStateSize (int slot) const noexcept
{
    if (slot < 0 || slot >= static_cast<int> (pluginStates.size()))
        return 0;

    const std::lock_guard<std::mutex> lock (pluginStateMutex);
    return static_cast<juce::int64> (
        pluginStates[static_cast<std::size_t> (slot)].data.getSize());
}

plugins::PluginInstrument* Session::getPluginInstrument (int slot) const noexcept
{
    if (slot < 0 || slot >= static_cast<int> (pluginInstruments.size()))
        return nullptr;

    const std::lock_guard<std::mutex> lock (pluginStateMutex);
    return pluginInstruments[static_cast<std::size_t> (slot)];
}

bool Session::getSlotFxChain (int slot,
                              AudioEngine::FxChainSnapshot& snapshot,
                              std::string& error) const
{
    const std::lock_guard<std::mutex> lock (fxMutex);

    if (! makeFxSnapshotLocked (slot, snapshot))
    {
        error = "patch slot is out of range";
        return false;
    }

    return true;
}

bool Session::addSlotFx (int slot,
                         const AudioEngine::FxInsertConfig& insert,
                         int position,
                         AudioEngine::FxChainSnapshot& snapshot,
                         std::string& error)
{
    const std::lock_guard<std::mutex> lock (fxMutex);

    if (! beginFxEditLocked (slot, error))
        return false;

    const bool added = fxRacks[static_cast<std::size_t> (slot)]->add (
        insert, position, sampleRate, maximumBlockSize, numChannels, error);

    if (added)
        rebuildFxChainLocked (slot);

    endFxEditLocked (slot);

    return added && makeFxSnapshotLocked (slot, snapshot);
}

bool Session::replaceSlotFx (int slot,
                             const std::vector<AudioEngine::FxInsertConfig>& chain,
                             AudioEngine::FxChainSnapshot& snapshot,
                             std::string& error)
{
    const std::lock_guard<std::mutex> lock (fxMutex);

    if (! beginFxEditLocked (slot, error))
        return false;

    const bool replaced = fxRacks[static_cast<std::size_t> (slot)]->replace (
        chain, sampleRate, maximumBlockSize, numChannels, error);

    if (replaced)
        rebuildFxChainLocked (slot);

    endFxEditLocked (slot);

    return replaced && makeFxSnapshotLocked (slot, snapshot);
}

bool Session::updateSlotFx (int slot,
                            int index,
                            const AudioEngine::FxInsertPatch& patch,
                            AudioEngine::FxChainSnapshot& snapshot,
                            std::string& error)
{
    const std::lock_guard<std::mutex> lock (fxMutex);

    if (! beginFxEditLocked (slot, error))
        return false;

    const bool updated = fxRacks[static_cast<std::size_t> (slot)]->update (
        index, patch, sampleRate, maximumBlockSize, numChannels, error);

    if (updated)
        rebuildFxChainLocked (slot);

    endFxEditLocked (slot);

    return updated && makeFxSnapshotLocked (slot, snapshot);
}

bool Session::removeSlotFx (int slot,
                            int index,
                            AudioEngine::FxChainSnapshot& snapshot,
                            std::string& error)
{
    const std::lock_guard<std::mutex> lock (fxMutex);

    if (! beginFxEditLocked (slot, error))
        return false;

    const bool removed = fxRacks[static_cast<std::size_t> (slot)]->remove (index, error);

    if (removed)
        rebuildFxChainLocked (slot);

    endFxEditLocked (slot);

    return removed && makeFxSnapshotLocked (slot, snapshot);
}

bool Session::reorderSlotFx (int slot,
                             const std::vector<int>& order,
                             AudioEngine::FxChainSnapshot& snapshot,
                             std::string& error)
{
    const std::lock_guard<std::mutex> lock (fxMutex);

    if (! beginFxEditLocked (slot, error))
        return false;

    const bool reordered = fxRacks[static_cast<std::size_t> (slot)]->reorder (order, error);

    if (reordered)
        rebuildFxChainLocked (slot);

    endFxEditLocked (slot);

    return reordered && makeFxSnapshotLocked (slot, snapshot);
}

bool Session::clearSlotFx (int slot,
                           AudioEngine::FxChainSnapshot& snapshot,
                           std::string& error)
{
    const std::lock_guard<std::mutex> lock (fxMutex);

    if (! beginFxEditLocked (slot, error))
        return false;

    fxRacks[static_cast<std::size_t> (slot)]->clear();
    rebuildFxChainLocked (slot);
    endFxEditLocked (slot);

    return makeFxSnapshotLocked (slot, snapshot);
}

void Session::updateFxTransport() noexcept
{
    const std::lock_guard<std::mutex> lock (fxMutex);
    const auto current = currentFxTransport();

    for (auto& rack : fxRacks)
        if (rack != nullptr)
            rack->setTransport (current);
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
        const int previous = activeSlot.load (std::memory_order_relaxed);

        if (previous >= 0)
            (void) capturePluginState (previous);

        // Nothing is armed any more: no editor should stay bound to the patch
        // that used to be.
        if (editorHost != nullptr)
            editorHost->closeEditorsExcept (-1);

        const std::lock_guard<std::mutex> lock (armMutex);
        switcher->disarm();
        requestedSlot.store (-1, std::memory_order_relaxed);
        pendingArmSlot.store (-1, std::memory_order_relaxed);
        activeSlot.store (-1, std::memory_order_relaxed);
        return;
    }

    if (slot >= getNumSlots())
        return;

    const int previous = activeSlot.load (std::memory_order_relaxed);

    if (previous >= 0 && previous != slot)
        (void) capturePluginState (previous);

    // Editor follow: only the armed patch may keep a window. The outgoing
    // editor is closed before the crossfade, so it can never keep showing the
    // previous patch. The new patch's window is opened explicitly later and
    // then shows the state restored by the factory/arm path.
    if (editorHost != nullptr)
        editorHost->closeEditorsExcept (slot);

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

void Session::setSlotGain (int slot, float linearGain) noexcept
{
    if (slot < 0 || slot >= static_cast<int> (config.slots.size()))
        return;

    const float clamped = juce::jlimit (0.0f, 4.0f, linearGain);
    config.slots[static_cast<std::size_t> (slot)].gain = clamped;

    if (engine != nullptr && slot < engine->getNumSlots())
        engine->getSlot (slot).setGain (clamped);
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

bool Session::pushMidi (const juce::MidiMessage& message) noexcept
{
    const auto size = message.getRawDataSize();

    if (size <= 0 || size > 3 || message.isSysEx())
        return false;

    const auto scope = midiFifo.write (1);

    if (scope.blockSize1 <= 0)
        return false;

    auto& event = midiQueue[static_cast<std::size_t> (scope.startIndex1)];
    event.size = static_cast<std::uint8_t> (size);

    const auto* raw = message.getRawData();

    for (int index = 0; index < size; ++index)
        event.data[static_cast<std::size_t> (index)] = raw[index];

    return true;
}

void Session::drainMidi (juce::MidiBuffer& destination) noexcept
{
    const auto ready = midiFifo.getNumReady();

    if (ready <= 0)
        return;

    const auto scope = midiFifo.read (ready);

    auto add = [this, &destination] (int start, int count)
    {
        for (int index = 0; index < count; ++index)
        {
            const auto& event = midiQueue[static_cast<std::size_t> (start + index)];

            if (event.size > 0)
                destination.addEvent (event.data.data(), static_cast<int> (event.size), 0);
        }
    };

    add (scope.startIndex1, scope.blockSize1);
    add (scope.startIndex2, scope.blockSize2);
}

void Session::renderInternal (int numSamples, juce::MidiBuffer& midi) noexcept
{
    const int samples = juce::jmin (numSamples, maximumBlockSize);

    if (samples <= 0 || engine == nullptr || switcher == nullptr)
        return;

    // MIDI inputs and the offline test seam feed this one lock-free queue. The
    // events are placed at the start of the block, which is the smallest
    // musically meaningful offset a block-based renderer can promise.
    drainMidi (midi);

    auto& bus = engine->getBus();

    bus.beginBlock (samples);

    if (switcher->process (samples, midi))
        bus.add (switcher->getOutputBuffer(), samples);

    bus.finish (renderBuffer, samples);

    // The transport counts rendered frames, so `/transport.positionSamples`
    // advances with the same block sizes the graph actually processed.
    transport.process (samples);

    updateMeters (samples);

    // Publish that a full block has rendered. An API chain edit waits for this
    // counter to advance after gating the slot, which proves the callback is
    // past the edit point and the chain vector is safe to rebuild.
    renderEpoch.fetch_add (1, std::memory_order_release);
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
