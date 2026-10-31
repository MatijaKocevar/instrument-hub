#pragma once

#include "audio/AudioEngine.h"
#include "audio/DeviceManager.h"
#include "audio/SlotSwitcher.h"
#include "audio/Transport.h"
#include "audio/WarmPool.h"
#include "instruments/Instrument.h"

#include <juce_audio_devices/juce_audio_devices.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace juce
{
class AudioPluginFormatManager;
}

namespace ihub::sampler
{
class LoadedKit;
}

namespace ihub
{

/** What produces audio for one slot of the composed session. */
enum class SessionSlotKind
{
    /** Built-in initialisation tone.

        The default set has no external samples or plugin bundles to rely on,
        so every slot starts with a phase-continuous test tone. It is a real
        `Instrument` / `AudioEngine::SlotSource` and follows the same audio
        thread rules as the sampler and plugin sources.
    */
    initTone,

    /** `SamplerInstrument` over a kit JSON file (`path`). */
    sampler,

    /** `PluginInstrument` over a VST3 bundle (`path`). */
    plugin,

    /** `AudioInputInstrument` for direct monitoring of the interface. */
    audioInput
};

const char* toString (SessionSlotKind kind) noexcept;

/** Warm-pool state of one logical slot, exposed through `/state`. */
enum class SessionWarmState
{
    cold,
    warming,
    warm,
    active,
    fading
};

const char* toString (SessionWarmState state) noexcept;

/** One instrument to load into a fixed engine slot. */
struct SessionSlotConfig
{
    /** Display name of the patch. */
    std::string name;
    SessionSlotKind kind = SessionSlotKind::initTone;

    /** Kit document (sampler) or VST3 bundle (plugin). */
    juce::String path;

    /** Frequency of the initialisation tone, in Hz. */
    double frequency = 220.0;

    /** Post-instrument gain, 0 .. 4. */
    float gain = 1.0f;

    /** Stereo balance, -1 (left) .. +1 (right). Published by the API; the
        graph has no panner yet. */
    float pan = 0.0f;

    /** Patch mute. Applied to the slot gain (muted patches are silent). */
    bool muted = false;

    /** Patch solo. When any patch is soloed, the others are silent. */
    bool solo = false;

    /** Resolved palette index, 0 .. 15. */
    int colorIndex = 0;

    /** Instrument descriptor published by `/state`. `kind` above is the audio
        source this session actually instantiates; a set may describe a plugin
        patch while the default session renders the initialisation tone. */
    std::string instrumentKind = "init-tone";
    std::string pluginFormat = "VST3";
    std::string pluginId;
    std::string pluginName;
    std::string pluginManufacturer;
    std::string pluginVersion;
    std::string samplePath;

    /** Interface channel copied by an audio-input slot. */
    int inputChannel = 0;

    /** Family and patch identity from the set document. Used by the API's
        family/patch tree; the audio path only cares about `kind`/`path`. */
    std::string family;
    std::string familyName;
    std::string patch;
    std::string patchName;
};

/** How and where the composed session opens its audio device. */
struct SessionDeviceConfig
{
    juce::String deviceTypeName;
    juce::String outputDeviceName;
    juce::String inputDeviceName;

    double sampleRate = 48000.0;
    int bufferSize = 128;
    int numOutputChannels = 2;
    int numInputChannels = 2;

    /** When true, only an ALSA `Loopback` output is accepted.

        Used by `--session-smoke` so the acceptance run cannot silently fall
        back to the desktop's default output device.
    */
    bool preferLoopback = false;
};

/** The complete default-session description. */
struct SessionConfig
{
    SessionDeviceConfig device;
    std::vector<SessionSlotConfig> slots;

    /** Equal-power crossfade length. 0 is the test seam for a hard switch. */
    float fadeMilliseconds = 3.0f;

    /** Slot to warm and arm around at startup. */
    int initialSlot = 0;

    /** Neighbours kept warm around the armed slot. The instance bound is
        `1 + neighbourCount` (clamped to at least one neighbour, so the
        previous patch can stay warm while a cold one loads). */
    int neighbourCount = AudioEngine::WarmPool::defaultNeighbourCount;

    /** Artificial delay before a cold patch is created, in milliseconds.

        Zero in production. The headless warm-pool acceptance uses it to make
        the `warming` -> `active` transition observable without a real plugin.
    */
    int loadDelayMilliseconds = 0;
};

/** Measurements from an offline armed-slot / slot-switch render. */
struct SessionSwitchReport
{
    int fromSlot = -1;
    int toSlot = -1;

    /** RMS of the armed slot before the switch, linear and in dBFS. */
    double rms = 0.0;
    double rmsDbfs = -1000.0;

    /** Largest sample-to-sample step seen anywhere in the measured span.

        This is the signal's ordinary slew plus any discontinuity; it is
        reported for context, not used as the click decision.
    */
    double maxSampleDelta = 0.0;
    double maxSampleDeltaDbfs = -1000.0;

    /** Largest discontinuity across the switch.

        The rendered switch is compared with the ideal equal-power crossfade
        of the same two source signals: each source is rendered in isolation
        from the same prepared state, then blended with the 3 ms product law.
        The value is the peak absolute difference between the real session
        output and that reference, in dBFS. A click, a hard switch or a broken
        ramp shows up as a large residual; ordinary signal curvature cancels.
    */
    double discontinuity = 0.0;
    double discontinuityDbfs = -1000.0;

    int samplesMeasured = 0;
    bool rendered = false;
};

/**
    The composed audio session.

    Owns the fixed `AudioEngine`, one `SlotSwitcher` over its logical slots,
    the `WarmPool` that instantiates patch instruments and the `DeviceManager`
    that streams the graph to hardware. The wiring is done once in `build()`;
    nothing is ever rewired at runtime.

    ## 64 logical slots, few live instruments

    The engine always has one slot per pad (64 in the default set), but a slot
    is not an instrument. Each engine slot is wired to a permanent, silent
    `Voice` proxy; the actual patch instrument is owned by the `WarmPool` and
    bound into the proxy with an atomic pointer store. Only the armed slot,
    its K neighbours and the previous patch are instantiated, bounded by
    `1 + K` live instances. Arming a warm slot is a pointer hand-over plus the
    3 ms equal-power crossfade; arming a cold slot leaves the previous patch
    audible while the pool loads the new one, reports `warming`, then
    crossfades when the instance is ready.

    ## Threading

    - `build()`, `prepare()`, `openDevice()`, `closeDevice()`, `resetGraph()`
      and `measureSwitch()` are message-thread calls; the stream must be
      stopped for the graph-editing ones.
    - `armSlot()` may load a cold patch through the warm pool, so it is a
      control-thread call (HTTP, controller, message thread), never the audio
      thread. Its audio-thread effect is only `SlotSwitcher::armSlot()`, one
      lock-free atomic publish, made once the instance is resident.
    - The getters are lock-free and safe while the stream runs.
    - `audioDeviceIOCallbackWithContext()` and `render()` are the audio path:
      no allocation, locks, logging or file I/O. All buffers are allocated in
      `prepare()` at the maximum block size.
*/
class Session final : public juce::AudioIODeviceCallback
{
public:
    /** One slot per Launchpad pad. */
    static constexpr int defaultLogicalSlots = 64;

    Session();
    ~Session() override;

    //==============================================================================
    // Composition (message thread)

    /** The 64-slot starter set.

        Every slot starts as an initialisation tone at a different pitch, so
        the composed session is audible without a plugin bundle or sample
        library. Slots are grouped into the eight families used by the
        Launchpad set file (eight patches each); load a set later to replace a
        tone with a sampler, plugin or audio-input source.
    */
    static std::vector<SessionSlotConfig> makeDefaultSet();

    /** Builds the fixed graph and the warm pool.

        Must run with the stream stopped. An empty `config.slots` uses
        `makeDefaultSet()`. On failure nothing is left half-built: the graph is
        released and `error` explains which slot failed. The initial slot and
        its neighbours are queued for warming in the background; arming the
        initial slot before they are ready reports `warming` and switches as
        soon as the instance is resident.
    */
    bool build (const SessionConfig& config, juce::String& error);

    /** Sizes every buffer and prepares the slots and their sources.

        Called by `build()` and again from `audioDeviceAboutToStart()` when the
        device format changes.
    */
    void prepare (double sampleRate, int maximumBlockSize, int numChannels);

    /** Returns every slot to idle and drops the pending arm request.

        The stream must be stopped. Warm instances are kept.
    */
    void resetGraph() noexcept;

    //==============================================================================
    // Device (message thread)

    /** Opens the configured output device and registers this session as its
        callback. `device.preferLoopback` restricts the search to the ALSA
        Loopback used by the hardware-free tests.
    */
    bool openDevice (juce::String* errorMessage = nullptr);

    void closeDevice();

    bool isDeviceOpen() const noexcept { return deviceOpen; }

    AudioEngine::DeviceManager& getDevices() noexcept { return devices; }

    //==============================================================================
    // Graph inspection

    bool isPrepared() const noexcept { return prepared; }
    int getNumSlots() const noexcept;

    /** Number of logical (pad) slots, even before the engine is built. */
    int getLogicalSlotCount() const noexcept
    {
        return static_cast<int> (config.slots.size());
    }

    AudioEngine::AudioEngine& getEngine() noexcept;
    const AudioEngine::AudioEngine& getEngine() const noexcept;

    AudioEngine::SlotSwitcher& getSwitcher() noexcept;
    const AudioEngine::SlotSwitcher& getSwitcher() const noexcept;

    /** The session transport. `render()` advances it by the frames it renders
        while it is playing, so `/transport` position is sample-accurate.

        Control-thread setters are lock-free; the audio thread only calls
        `process()`.
    */
    AudioEngine::Transport& getTransport() noexcept { return transport; }
    const AudioEngine::Transport& getTransport() const noexcept { return transport; }

    /** Most recent crossfade-weighted peak of one slot, 0 when silent. */
    float getSlotMeterPeak (int slot) const noexcept;

    /** Most recent peak of the mixed master bus. */
    float getMasterMeterPeak() const noexcept;

    const SessionConfig& getConfig() const noexcept { return config; }
    const std::vector<SessionSlotConfig>& getSlotConfigs() const noexcept
    {
        return config.slots;
    }

    /** The slot's total instrument + FX latency, in samples. */
    int getSlotLatencySamples (int slot) const noexcept;

    //==============================================================================
    // Warm-pool inspection (safe on any non-audio thread)

    int getNeighbourCount() const noexcept;
    int getWarmInstanceLimit() const noexcept;
    int getLiveInstrumentCount() const noexcept;

    /** Peak number of simultaneously live patch instruments. The acceptance
        bound is `getWarmInstanceLimit()`. */
    int getPeakInstrumentCount() const noexcept;

    bool isSlotWarm (int slot) const noexcept;

    /** Cold, warming, warm, active or fading. `active` means the slot is the
        current arm target; `warming` means a cold instance is being created. */
    SessionWarmState getWarmState (int slot) const noexcept;

    /** The warm-pool state as stable text, for `/state` and tests. */
    const char* getWarmStateName (int slot) const noexcept;

    /** Polls until the slot's instance is resident (it may still be a
        neighbour rather than the armed slot). For tests and offline tools. */
    bool waitForSlotWarm (int slot, int timeoutMs = 5000);

    /** Polls until a pending cold arm has been assigned its instance and the
        switcher has been asked to crossfade. For tests and offline tools. */
    bool waitForSlotActive (int slot, int timeoutMs = 5000);

    //==============================================================================
    // Control (control thread; never the audio thread)

    /** Requests an equal-power fade to `slot`.

        A warm slot switches at the next block boundary. A cold slot leaves the
        current patch playing, warms in the background, then crossfades in.
        Negative indices disarm.
    */
    void armSlot (int slot);

    /** The most recently requested slot, including a cold slot still warming.
        -1 when disarmed. */
    int getArmedSlot() const noexcept;

    /** Publishes a patch's effective linear gain (already including mute and
        solo) to its fixed engine slot. Lock-free atomic store; call from a
        control thread, never the audio thread. */
    void setSlotGain (int slot, float linearGain) noexcept;

    //==============================================================================
    // Audio thread / offline rendering

    /** Renders `numSamples` of the session into `output`.

        Real-time safe after `prepare()`. Used by the offline smoke render; the
        device callback follows the same path.
    */
    void render (juce::AudioBuffer<float>& output,
                 juce::MidiBuffer& midi,
                 int numSamples) noexcept;

    /** Renders a deterministic arm / switch / measure pass.

        Runs with the stream stopped: resets the graph, arms `fromSlot`, lets
        its fade-in settle, measures its RMS, switches to `toSlot`, and compares
        the rendered switch with the ideal 3 ms equal-power crossfade of the
        same two sources. `rmsDbfs` and `discontinuityDbfs` are relative to
        full scale.
    */
    SessionSwitchReport measureSwitch (int fromSlot,
                                       int toSlot,
                                       int settleBlocks = 8,
                                       int rmsBlocks = 8,
                                       int switchBlocks = 32);

    //==============================================================================
    // juce::AudioIODeviceCallback

    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                           int numInputChannels,
                                           float* const* outputChannelData,
                                           int numOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override;

private:
    /** Permanent, real-time-safe proxy wired to one fixed engine slot. */
    struct Voice;

    std::unique_ptr<AudioEngine::SlotSource> createPooledSource (int slotIndex,
                                                                 juce::String& error);
    void warmCallback (int slot);
    void bindVoice (int slot, AudioEngine::SlotSource* instance) noexcept;
    void clearVoice (AudioEngine::SlotSource* instance) noexcept;
    bool finishArmLocked (int slot, AudioEngine::SlotSource* pinnedInstance);
    bool warmSlotBlockingLocked (int slot, int timeoutMs);
    bool waitForWarmIdle (int timeoutMs);
    void releaseAllPinsLocked();
    void shutdownWarmPool() noexcept;

    bool resolveDevice (AudioEngine::DeviceSetup& setup, juce::String& error);
    void renderInternal (int numSamples, juce::MidiBuffer& midi) noexcept;
    void updateMeters (int numSamples) noexcept;
    void copyToOutput (float* const* outputChannelData,
                       int numOutputChannels,
                       int numSamples) noexcept;

    SessionConfig config;

    // Destruction order matters: the plugin format manager must outlive the
    // instruments, and the engine must be destroyed before the proxies its
    // slots point at. The destructor tears the pool down explicitly.
    std::unique_ptr<juce::AudioPluginFormatManager> pluginFormats;
    std::unique_ptr<AudioEngine::WarmPool> warmPool;
    std::vector<std::unique_ptr<Voice>> voices;

    std::unique_ptr<AudioEngine::AudioEngine> engine;
    std::unique_ptr<AudioEngine::SlotSwitcher> switcher;
    AudioEngine::DeviceManager devices;
    AudioEngine::Transport transport;

    juce::AudioBuffer<float> renderBuffer;
    juce::MidiBuffer blockMidi;

    // Peak meters published by the audio thread. Allocated once in prepare().
    std::unique_ptr<std::atomic<float>[]> slotMeterPeaks;
    std::atomic<float> masterMeterPeak { 0.0f };
    int meterSlotCount = 0;

    // Warm-pool arm hand-off. The audio thread never touches these; the
    // switcher request itself is the only lock-free publish.
    mutable std::mutex armMutex;
    std::atomic<int> requestedSlot { -1 };
    std::atomic<int> activeSlot { -1 };
    std::atomic<int> pendingArmSlot { -1 };
    int pinnedActiveSlot = -1;
    int pinnedPreviousSlot = -1;

    double sampleRate = 48000.0;
    int maximumBlockSize = 0;
    int numChannels = 2;
    bool prepared = false;
    bool deviceOpen = false;
    bool callbackInstalled = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Session)
};

} // namespace ihub
