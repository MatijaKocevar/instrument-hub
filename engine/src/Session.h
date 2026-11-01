#pragma once

#include "audio/AudioEngine.h"
#include "audio/DeviceManager.h"
#include "audio/FxRack.h"
#include "audio/SlotSwitcher.h"
#include "instruments/Instrument.h"
#include "session/DeviceController.h"
#include "session/FxChainController.h"
#include "session/MeterBridge.h"
#include "session/MidiBridge.h"
#include "session/PluginStateStore.h"
#include "session/TransportControl.h"
#include "session/WarmPoolController.h"
#include "SessionConfig.h"
#include "SessionSwitchReport.h"

#include <juce_audio_devices/juce_audio_devices.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace juce
{
class AudioPluginFormatManager;
}

namespace ihub::plugins
{
class PluginInstrument;
}

namespace ihub::session
{
class EditorHost;
}

namespace ihub
{


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

        Every slot starts unassigned (`empty`) and is therefore silent. Slots
        are grouped into the eight families used by the Launchpad set file
        (eight patches each); load a set to assign a sampler, plugin, audio
        input or initialisation tone to a patch.
    */
    static std::vector<SessionSlotConfig> makeDefaultSet
        (SessionSlotKind defaultKind = SessionSlotKind::empty);

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

    bool isDeviceOpen() const noexcept { return devices.isOpen(); }

    AudioEngine::DeviceManager& getDevices() noexcept { return devices.getManager(); }

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
    AudioEngine::Transport& getTransport() noexcept
    {
        return transportControl.getTransport();
    }
    const AudioEngine::Transport& getTransport() const noexcept
    {
        return transportControl.getTransport();
    }

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
    // Instrument source (control thread, T124)

    /** Replaces one slot's instrument source.

        Only the source fields of `source` are taken: the patch identity, name,
        mixer state, colour and insert chain stay with the slot. The slot's warm
        request gets a new id, so the previous instance is retired (its destroy
        hook clears the fixed voice) and a fresh one is created. An armed slot
        is re-armed as soon as the new instance is resident, so the change is
        audible after a short gap; a cold slot picks the new source up the next
        time it is armed.

        Must run on a control thread, never the audio thread. Returns false with
        `error` when the session is not built or the slot is invalid.
    */
    bool setSlotSource (int slot,
                        const SessionSlotConfig& source,
                        std::string& error);

    /** Called after `setSlotSource()` accepts a new assignment.

        The application uses it to persist the Instrument page's picker choice.
        Runs on the control thread that made the change, after the live swap is
        queued; the audio thread never calls it. */
    using SourceAssignedHook = std::function<void (int slot,
                                                   const SessionSlotConfig& source)>;
    void setSourceAssignedHook (SourceAssignedHook hook);

    //==============================================================================
    // Native plugin editor (control thread, T129)

    /** Opens, or brings to the front, the native editor of a patch's hosted
        plugin.

        The patch's `PluginInstrument` is resolved from the live warm-pool
        instance and the call is marshalled to the message thread, where JUCE
        requires every editor and window to live. One window is kept per slot:
        opening an already-open editor reuses it, opening after a source swap
        creates a new one.

        Only a plugin patch that is resident has an editor. A cold or non-plugin
        patch returns false with a clear `error`; an editor is never opened
        implicitly by selecting, arming or switching a patch.
    */
    bool openEditor (int slot, std::string& error);

    /** Closes one slot's editor window. Idempotent: closing a slot with no
        editor succeeds. Returns false only for an invalid slot. */
    bool closeEditor (int slot, std::string& error);

    /** True while this slot's native editor window is on screen. */
    bool isEditorOpen (int slot) const noexcept;

    /** Number of native editor windows the session currently keeps open. */
    int getOpenEditorCount() const noexcept;

    //==============================================================================
    // Per-patch plugin state (control thread, T130)

    /** Points the patch-state sidecar at a directory next to `set.yaml`.

        The directory is created on the first capture. Any existing
        `slot-<n>.bin` files are read immediately, so calling this before
        `build()` means the initial warm-pool instances are created with their
        restored state. Passing an empty file clears the directory without
        touching anything on disk.

        The sidecar is one binary file per patch; the opaque plugin blob is
        never inlined in JSON (DECISIONS: persistence).
    */
    void setPluginStateDirectory (const juce::File& directory);

    juce::File getPluginStateDirectory() const
    {
        return pluginState.getDirectory();
    }

    /** Re-reads every `slot-<n>.bin` from the current directory. */
    bool loadPluginStates();

    /** Writes every in-memory patch blob to the current directory.

        Captures already persist themselves; this is for an explicit save and
        for tests. Returns false when the directory is not set or a write
        failed.
    */
    bool savePluginStates();

    /** Captures a live plugin patch's `getStateInformation()` into its blob
        and writes the sidecar immediately.

        Also called by the session when a patch is switched away from, when its
        editor closes, and before the warm pool evicts it. Returns false when
        the slot has no resident plugin instance. Capturing unchanged state is
        a no-op, so the repeated calls are cheap.
    */
    bool capturePluginState (int slot);

    /** True when this slot has a stored plugin blob. */
    bool hasPluginState (int slot) const noexcept;

    /** Size of the slot's stored plugin blob in bytes, 0 when none. */
    juce::int64 getPluginStateSize (int slot) const noexcept;

    /** The live hosted plugin for a slot, or nullptr.

        Inspection only: the pointer is borrowed and may be retired by the
        warm pool at any time. Used by the UI/diagnostics and by the state
        tests to set a preset before a capture.
    */
    plugins::PluginInstrument* getPluginInstrument (int slot) const noexcept;

    //==============================================================================
    // Per-patch insert chain (control thread)
    /** Reads one patch's insert chain and latency budget. */
    bool getSlotFxChain (int slot,
                         AudioEngine::FxChainSnapshot& snapshot,
                         std::string& error) const;

    /** Appends or inserts an effect. `position < 0` appends. */
    bool addSlotFx (int slot,
                    const AudioEngine::FxInsertConfig& insert,
                    int position,
                    AudioEngine::FxChainSnapshot& snapshot,
                    std::string& error);

    /** Replaces the whole chain in one operation. */
    bool replaceSlotFx (int slot,
                        const std::vector<AudioEngine::FxInsertConfig>& chain,
                        AudioEngine::FxChainSnapshot& snapshot,
                        std::string& error);

    /** Changes bypass, wet/dry, parameters or the position of one insert. */
    bool updateSlotFx (int slot,
                       int index,
                       const AudioEngine::FxInsertPatch& patch,
                       AudioEngine::FxChainSnapshot& snapshot,
                       std::string& error);

    bool removeSlotFx (int slot,
                       int index,
                       AudioEngine::FxChainSnapshot& snapshot,
                       std::string& error);

    bool reorderSlotFx (int slot,
                        const std::vector<int>& order,
                        AudioEngine::FxChainSnapshot& snapshot,
                        std::string& error);

    bool clearSlotFx (int slot,
                      AudioEngine::FxChainSnapshot& snapshot,
                      std::string& error);

    /** Publishes the transport to every tempo-synced delay insert. */
    void updateFxTransport() noexcept;

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

    /** Queues one short MIDI message for the next audio block.

        Called from a MIDI input thread. The event goes into a fixed-capacity
        lock-free SPSC ring; the audio callback drains it into the block's
        `MidiBuffer` at sample 0. Nothing here allocates, locks or logs, so it
        is safe to call from a juce::MidiInput callback while the stream runs.
        Returns false when the queue is full or the message is too long (SysEx
        is deliberately not forwarded).
    */
    bool pushMidi (const juce::MidiMessage& message) noexcept;

    /** Number of MIDI events waiting for the next audio block. Test seam. */
    int getQueuedMidiCount() const noexcept
    {
        return midi.getQueuedCount();
    }

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
    std::unique_ptr<AudioEngine::SlotSource> createPooledSource (int slotIndex,
                                                                 juce::String& error);

    void renderInternal (int numSamples, juce::MidiBuffer& midi) noexcept;
    void copyToOutput (float* const* outputChannelData,
                       int numOutputChannels,
                       int numSamples) noexcept;

    // Per-patch plugin state helper (T130). Runs on a control thread.
    void capturePluginStateForInstance (AudioEngine::SlotSource& instance);
    bool pluginIdentityForSlot (int slot, session::PluginStateIdentity& identity) const;

    /** True while the device callback is installed and rendering. */
    bool isStreamActive() const noexcept
    {
        return devices.isOpen() && devices.hasCallback();
    }

    SessionConfig config;

    /** Source fields the warm-pool factory reads, guarded separately so the
        control thread can swap a source while another slot's instance is being
        created on the pool worker. */
    std::vector<SessionSlotConfig> sourceConfigs;
    mutable std::mutex sourceMutex;
    std::uint64_t sourceRevision = 0;

    SourceAssignedHook sourceAssignedHook;
    mutable std::mutex sourceAssignedMutex;

    /** Editor windows and their live-instrument registry (T129). */
    std::unique_ptr<session::EditorHost> editorHost;

    /** One opaque plugin-state blob per patch (T130), plus the live
        instrument registry the warm-pool factory fills. */
    session::PluginStateStore pluginState { defaultLogicalSlots };

    // Destruction order matters: the plugin format manager must outlive the
    // instruments, and the engine must be destroyed before the proxies its
    // slots point at. The destructor tears the pool down explicitly.
    std::unique_ptr<juce::AudioPluginFormatManager> pluginFormats;
    session::WarmPoolController warmPool;
    session::FxChainController fxChain;

    std::unique_ptr<AudioEngine::AudioEngine> engine;
    std::unique_ptr<AudioEngine::SlotSwitcher> switcher;
    session::DeviceController devices;
    session::TransportControl transportControl;
    session::MeterBridge meters;
    session::MidiBridge midi;

    juce::AudioBuffer<float> renderBuffer;

    double sampleRate = 48000.0;
    int maximumBlockSize = 0;
    int numChannels = 2;
    bool prepared = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Session)
};

} // namespace ihub
