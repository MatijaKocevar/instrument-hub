#pragma once

#include <juce_core/juce_core.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace Api
{

//==============================================================================
// Control API state
//==============================================================================

/** Static description of one audio slot. */
struct SlotSnapshot
{
    int index = 0;
    std::string name;
    bool armed = false;

    /** Audio-thread state of the fixed graph slot: idle, warming, active or
        fading. Supplied by a runtime provider when one is attached; "idle"
        otherwise.
    */
    std::string state = "idle";

    /** Warm-pool state: cold, warming, warm, active or fading. A cold patch is
        a descriptor only; `warming` means its instrument is being created. */
    std::string warmState = "cold";
    bool warm = false;

    /** Family/patch identity from the set document. */
    std::string family;
    std::string familyName;
    std::string patch;
    std::string patchName;

    /** Crossfade gain published by `SlotSwitcher`, 0 .. 1. */
    float gain = 0.0f;

    /** Instrument + FX latency of this slot's chain. */
    int latencySamples = 0;
    /** The same total at the sample rate it was measured at. */
    double latencyMs = 0.0;

    /** True while this slot's native editor is open (not serialised in `slots`). */
    bool editorOpen = false;
};

/** Instrument descriptor published with every patch in the family tree.

    The descriptor mirrors the patch's declared source. `POST
    /patches/{slot}/source` replaces it together with the live source the
    session instantiates. It is present for every patch, hot or cold, so the
    grid can show a plugin/sampler patch before its instance is loaded.
*/
struct PatchInstrumentState
{
    std::string id;
    std::string name;
    std::string kind = "empty";
    std::string pluginFormat = "VST3";
    std::string pluginId;
    std::string pluginName;
    std::string pluginManufacturer;
    std::string pluginVersion;
    std::string samplePath;
};

/** A requested instrument-source change for one patch (T124).

    Only `kind` is required. `path` is the kit document for `sampler` and the
    VST3 bundle for `plugin`; `frequency` exists for `init-tone` and
    `inputChannel` for `audio-input`. The descriptor fields are optional
    overrides the inventory supplies.
*/
struct PatchSourceRequest
{
    std::string kind = "empty";
    std::string name;
    std::string path;
    double frequency = 0.0;
    int inputChannel = 0;
    std::string pluginFormat = "VST3";
    std::string pluginId;
    std::string pluginName;
    std::string pluginManufacturer;
    std::string pluginVersion;
    std::string samplePath;
};

/** One patch in the family tree exposed by `GET /state`.

    Every patch carries its instrument descriptor, mixer state, resolved colour
    and live warm/latency values, so the UI can render the whole grid from one
    state document.
*/
struct PatchState
{
    int slot = -1;
    std::string id;
    std::string name;
    std::string family;
    std::string familyName;

    PatchInstrumentState instrument;

    /** Patch volume in dB, -60 .. +12. */
    double volumeDb = 0.0;
    /** Stereo balance, -1 (left) .. +1 (right). */
    double pan = 0.0;
    bool mute = false;
    bool solo = false;

    /** Resolved palette index, 0 .. 15 (family default or patch override). */
    int colorIndex = 0;

    /** Live warm-pool state: cold, warming, warm, active or fading. */
    std::string warmState = "cold";
    bool warm = false;

    /** True while the patch's native plugin editor window is on screen. */
    bool editorOpen = false;

    /** Instrument + FX latency of this patch's chain. */
    int latencySamples = 0;
    double latencyMs = 0.0;
};

/** One instrument family in the family tree. */
struct FamilyState
{
    std::string id;
    std::string name;
    std::vector<PatchState> patches;
};

/** Result of an explicit save of the running plugin's sound into a patch. */
struct PatchStateCapture
{
    int slot = -1;
    bool captured = false;

    /** Size of the patch's opaque state blob after the capture, in bytes. */
    juce::int64 sizeBytes = 0;
};

//==============================================================================
// Per-patch insert chain (T121)

/** One insert as returned by the FX chain API. */
struct FxInsertState
{
    int index = 0;

    /** Stable effect type: `eq`, `compressor`, `delay`, `reverb` or `vst3`. */
    std::string type;

    /** `builtin` or `vst3`. */
    std::string kind;

    /** Built-in id, or a plugin's bundle path. */
    std::string id;
    std::string name;

    bool bypassed = false;

    /** Wet/dry balance, 0 (dry) .. 1 (wet). */
    double wet = 1.0;

    int latencySamples = 0;

    /** Effect-specific values, read-only unless the effect supports writes. */
    std::map<std::string, double> parameters;
};

/** A patch's whole insert chain and latency budget. */
struct FxChainState
{
    int slot = -1;
    int instrumentLatencySamples = 0;
    int fxLatencySamples = 0;
    int latencySamples = 0;
    double latencyMs = 0.0;
    std::vector<FxInsertState> inserts;
};

/** A value to append or replace in a patch's chain. */
struct FxInsertRequest
{
    std::string type;
    std::string path;
    std::string id;
    std::string name;
    bool bypassed = false;
    double wet = 1.0;

    /** -1 appends; otherwise the new insert's 0-based position. */
    int position = -1;

    std::map<std::string, double> parameters;
};

/** A partial update of one insert. */
struct FxUpdateRequest
{
    std::optional<bool> bypassed;
    std::optional<double> wet;
    std::optional<int> position;

    /** When present, the effect's parameters are replaced by these values. */
    std::optional<std::map<std::string, double>> parameters;
};

enum class FxCommandKind
{
    add,
    replace,
    update,
    remove,
    reorder,
    clear
};

/** One mutation of a patch's insert chain. */
struct FxCommand
{
    FxCommandKind kind = FxCommandKind::add;

    /** Target insert for update/remove. */
    int index = -1;

    /** For add: the insert to append. For replace: the whole new chain. */
    std::vector<FxInsertRequest> inserts;

    /** For reorder: `order[0]` becomes the first insert. */
    std::vector<int> order;

    FxUpdateRequest update;
};

using FxChainProvider = std::function<bool (int slot, FxChainState&, std::string&)>;
using FxCommandHandler = std::function<bool (int slot,
                                             const FxCommand&,
                                             FxChainState&,
                                             std::string&)>;

/** Mutable settings exposed by `GET /settings` and `POST /settings`.

    The values are deliberately engine-agnostic: a later task applies them to
    the audio graph. Keeping them here means the API contract is stable while
    the graph grows.
*/
struct SettingsState
{
    double masterGain = 0.8;
    double inputGain = 1.0;
    int midiChannel = 1;
    bool metronome = false;
};

/** Sample-accurate transport values exposed by `POST /transport` and `/state`. */
struct TransportState
{
    bool playing = false;
    double bpm = 120.0;
    double ppq = 0.0;
    std::int64_t positionSamples = 0;
};

/** Live values for one slot, read from the audio graph rather than the store.

    A provider that reads a running session must only touch lock-free/atomic
    values; `snapshot()` calls it on the HTTP thread, never from the audio
    callback.
*/
struct SlotRuntimeState
{
    bool armed = false;
    float gain = 0.0f;
    std::string state = "idle";
    std::string warmState = "cold";
    bool warm = false;
    bool editorOpen = false;
};

/** Supplies one slot's live state. */
using SlotRuntimeProvider = std::function<SlotRuntimeState (int slot)>;

/** Supplies the live transport. */
using TransportProvider = std::function<TransportState()>;

/** A consistent-enough snapshot of everything `GET /state` returns. */
struct StateSnapshot
{
    int schemaVersion = 1;
    std::string applicationName = "Instrument Hub";
    std::string applicationVersion = "0.1.0";

    int armedSlot = -1;
    std::vector<SlotSnapshot> slots;

    /** 8 families x 8 patches for the Launchpad's 64-pad grid. */
    std::vector<FamilyState> families;

    SettingsState settings;
    TransportState transport;

    double uptimeMs = 0.0;
};

/** One slot in a meter frame. */
struct SlotMeter
{
    int slot = 0;
    float peak = 0.0f;
    float gain = 0.0f;
    bool armed = false;
};

/** One coalesced meter frame, broadcast at `meterHz` (30 by default). */
struct MeterFrame
{
    std::uint64_t sequence = 0;
    double timestampMs = 0.0;
    float masterPeak = 0.0f;
    std::vector<SlotMeter> slots;
};

/** Supplies meter values to the broadcaster.

    Called on the broadcaster thread, never from the audio thread. A provider
    that reads an engine must only read lock-free/atomic values: the API server
    must not add locks to the audio path.
*/
using MeterProvider = std::function<MeterFrame()>;

//==============================================================================
// Audio device settings (T053)
//==============================================================================

/** One physical channel exposed by the open device. */
struct AudioChannelState
{
    int index = 0;
    std::string name;
    bool active = false;
};

/** One (type, device) pair the engine can open. */
struct AudioDeviceState
{
    std::string type;
    std::string name;
};

/** The latency budget shown in the audio settings panel, in milliseconds.

    `bufferMs` is one device buffer, `deviceMs` is the input + output latency
    JUCE reports for the open device, `chainMs` is the summed instrument + FX
    latency of the longest slot chain, and `totalMs` is their sum.
*/
struct AudioLatencyState
{
    double bufferMs = 0.0;
    double deviceMs = 0.0;
    double chainMs = 0.0;
    double totalMs = 0.0;
};

/** Everything `GET /audio` returns. */
struct AudioStateSnapshot
{
    bool open = false;

    std::string deviceType;
    std::string deviceName;

    std::vector<std::string> deviceTypes;
    std::vector<AudioDeviceState> devices;

    double sampleRate = 0.0;
    int bufferSize = 0;

    std::vector<double> sampleRates;
    std::vector<int> bufferSizes;

    std::vector<AudioChannelState> inputChannels;
    std::vector<AudioChannelState> outputChannels;

    AudioLatencyState latency;

    /** Device driver xruns. */
    std::uint64_t deviceXruns = 0;
    /** Callback overruns counted by TimingMonitor. */
    std::uint64_t callbackOverruns = 0;
    /** Total xruns (`deviceXruns + callbackOverruns`). */
    std::uint64_t xruns = 0;

    std::uint64_t callbacks = 0;
    double loadPercent = 0.0;
};

/** A partial audio settings update from `POST /audio`. */
struct AudioSettingsPatch
{
    std::optional<std::string> deviceType;
    std::optional<std::string> deviceName;
    std::optional<double> sampleRate;
    std::optional<int> bufferSize;
    std::optional<std::vector<int>> inputChannels;
    std::optional<std::vector<int>> outputChannels;
};

/** The result of `POST /audio/measure`. */
struct AudioMeasurementState
{
    bool valid = false;
    int roundTripSamples = 0;
    double roundTripMs = 0.0;
    double expectedMs = 0.0;
    float confidence = 0.0f;
    double sampleRate = 0.0;
    int bufferSize = 0;
    std::string error;
};

/** One installed plug-in as returned by `GET /plugins` (T122).

    The inventory is read-only. Assigning a plug-in to a patch happens through
    the per-patch instrument/FX endpoints, not here.
*/
struct PluginState
{
    std::string id;
    std::string name;
    std::string description;
    std::string vendor;
    std::string version;
    std::string category;

    /** True when the plug-in identifies itself as an instrument. */
    bool isInstrument = false;

    /** True for an effect (the complement of `isInstrument`). */
    bool isEffect = true;

    std::string format;

    /** Bundle path (`.../Name.vst3`). */
    std::string path;
};

/** Everything `GET /plugins` returns. */
struct PluginInventoryState
{
    std::vector<PluginState> plugins;
    int total = 0;
    std::string query;
    bool scanning = false;
    bool scanned = false;
    std::int64_t lastScanMs = 0;
    std::string error;
};

/** Supplies a filtered inventory snapshot. Called on an HTTP thread. */
using PluginInventoryProvider =
    std::function<PluginInventoryState (const std::string& search)>;

/** Starts a background rescan. Returns false when one is already running. */
using PluginRescanHandler = std::function<bool()>;

//==============================================================================
// MIDI inputs and the pad test (T126)
//==============================================================================

/** One MIDI input as returned by `GET /midi`.

    `id` is the JUCE device identifier (stable across resumes), `role` is
    `launchpad`, `keystation` or `input`, and `enabled` is the user's switch.
    `open` is true when the engine currently owns the port; for the Launchpad
    that means the driver is connected.
*/
struct MidiInputState
{
    std::string id;
    std::string name;
    std::string role = "input";

    /** Short port label (`MI`, `DA`, `USB`) when the device name carries one. */
    std::string port;

    bool enabled = false;
    bool open = false;
    std::uint64_t events = 0;
    std::string lastEvent;
};

/** Status of one named controller port (Launchpad MI, Keystation USB). */
struct MidiPortState
{
    /** Device name, empty when the device is not present. */
    std::string name;

    /** Short port label: `MI`, `DA` or `USB`. */
    std::string port;

    bool connected = false;

    /** True once the controller echoed its programmer/session-mode frame. */
    bool handshake = false;

    std::string inputName;
    std::string outputName;
    std::string error;
};

/** Result of the most recent pad test. */
struct MidiPadTestState
{
    bool active = false;
    int lit = 0;
    int durationMs = 0;
};

/** Everything `GET /midi` returns. */
struct MidiStateSnapshot
{
    std::vector<MidiInputState> inputs;
    MidiPortState launchpad;
    MidiPortState keystation;
    MidiPadTestState padTest;
    std::string error;
};

/** Supplies the current MIDI device and controller state. Called on an HTTP
    thread; a live implementation must not block the audio callback. */
using MidiStateProvider = std::function<MidiStateSnapshot()>;

/** Enables or disables one MIDI input. Returns false with a message when the
    port cannot be opened. */
using MidiInputHandler = std::function<bool (const std::string& id,
                                             bool enabled,
                                             std::string& error)>;

/** Runs the Launchpad pad test. Returns false with a message when there is no
    connected Launchpad. */
using MidiPadTestHandler = std::function<bool (std::string& error)>;

/** Supplies the current audio state. Called on an HTTP thread. */
using AudioStateProvider = std::function<AudioStateSnapshot()>;

/** Applies a settings patch. Returns false with a message on rejection. */
using AudioApplyHandler = std::function<bool (const AudioSettingsPatch&, std::string&)>;

/** Runs the latency probe. Blocks for up to a few seconds. */
using AudioMeasureHandler = std::function<AudioMeasurementState()>;

/**
    Thread-safe control state behind the HTTP API.

    Every method is safe to call from the HTTP server threads, the UI and the
    message thread. The store never touches the audio thread; `setArmHandler()`
    and `setTransportHandler()` bridge requests into the lock-free graph
    controls, and the runtime providers let `snapshot()` read the real session
    state without putting a lock on the audio path.
*/
class StateStore
{
public:
    explicit StateStore (int numSlots = 64);

    //==============================================================================
    // Arm

    int getArmedSlot() const noexcept;

    /** Arms `slot`, or disarms when `slot` is negative.

        Returns false (and changes nothing) when `slot` is not a valid index and
        not negative.
    */
    bool setArmedSlot (int slot);

    /** Optional listener called after a successful `setArmedSlot()`.

        It runs outside the store's lock. The handler must be lock-free if it
        crosses into the audio graph; `SlotSwitcher::armSlot()` is the intended
        target.
    */
    using ArmHandler = std::function<void (int slot)>;
    void setArmHandler (ArmHandler handler);

    //==============================================================================
    // Settings / transport

    SettingsState getSettings() const;
    bool updateSettings (const juce::var& patch, std::string& error);

    TransportState getTransport() const;
    bool updateTransport (const juce::var& patch, std::string& error);

    /** Applies the accepted patch to the audio graph.

        `next` is the merged store state and `patch` is the original JSON
        object, so the handler can apply only the fields the request changed
        and leave a running position alone when the request only toggles
        `playing`.
    */
    using TransportHandler = std::function<void (const TransportState& next,
                                                 const juce::var& patch)>;
    void setTransportHandler (TransportHandler handler);

    /** Reads `GET /transport` and `/state` from the session instead of the
        synthetic store values. Must be lock-free if the session is running.
    */
    void setTransportProvider (TransportProvider provider);

    /** Reads each slot's armed flag, crossfade gain and graph state from the
        session. Replaces the synthetic `armed` value in `snapshot()`.
    */
    void setSlotRuntimeProvider (SlotRuntimeProvider provider);

    //==============================================================================
    // Change notification (T103)

    /** Called after every successful mutation of the store.

        `kind` is `"arm"`, `"settings"`, `"transport"`, `"slot"` or `"patch"`;
        `slot` is the affected slot for `"arm"`/`"slot"`/`"patch"` and -1
        otherwise. The handler runs outside the store's lock, on the thread that
        made the change, so it must not block the audio path (it never is on
        it).

        `StateBroadcast` installs this to mirror API and controller changes to
        the connected clients. There is one handler per store.
    */
    using ChangeHandler = std::function<void (const char* kind, int slot)>;
    void setChangeHandler (ChangeHandler handler);

    //==============================================================================
    // Slots

    int getNumSlots() const noexcept;
    void setSlotName (int slot, std::string name);
    void setSlotIdentity (int slot,
                          std::string family,
                          std::string familyName,
                          std::string patch,
                          std::string patchName);

    /** Publishes the latency of one slot's instrument + FX chain.

        Pass the slot's current sample rate; it is used for the millisecond
        readout. Values are clamped to zero and ignored for an invalid slot.
        Call from the control/message thread after a chain change, never from
        the audio callback.
    */
    void setSlotLatency (int slot, int samples, double sampleRate);

    /** Publishes the family -> patch tree that `/state` mirrors. */
    void setFamilyTree (std::vector<FamilyState> tree);
    std::vector<FamilyState> getFamilyTree() const;

    //==============================================================================
    // Patch mix / colour (T120)

    /** Seeds the instrument descriptor of one patch. Called when a set is
        composed; it does not broadcast a change. Ignored for an invalid slot.
    */
    void setPatchInstrument (int slot, PatchInstrumentState instrument);

    /** Seeds the persistent mixer/colour state of one patch.

        Values are clamped to the API ranges (`volumeDb` -60..12, `pan` -1..1,
        `colorIndex` 0..15). Called when a set is composed; it does not
        broadcast a change.
    */
    void setPatchMix (int slot,
                      double volumeDb,
                      double pan,
                      bool mute,
                      bool solo,
                      int colorIndex);

    /** The stored patch, without live warm/latency values. */
    PatchState getPatch (int slot) const;

    /** Applies a partial patch update from `PATCH /patches/{slot}`.

        Accepts `volumeDb`, `pan`, `mute` (or `muted`), `solo` and `colorIndex`.
        Unknown fields and out-of-range values are rejected before anything is
        applied. On success every slot's effective gain is recomputed (mute and
        solo are global) and the patch handler is called once per slot.
    */
    bool updatePatch (int slot, const juce::var& patch, std::string& error);

    /** Publishes the source handler used by `POST /patches/{slot}/source`.

        The handler swaps the live source in the fixed graph. It runs outside
        the store's lock, on the HTTP thread, and must not run on the audio
        thread. Empty means no audio session is attached.
    */
    using PatchSourceHandler = std::function<bool (int slot,
                                                   const PatchSourceRequest& source,
                                                   std::string& error)>;
    void setPatchSourceHandler (PatchSourceHandler handler);

    /** Applies one instrument-source change and its descriptor update.

        Calls the source handler first; nothing is changed when it refuses the
        request. On success the patch descriptor is replaced, the change is
        broadcast as `event.kind: "patch"`, and the source is returned in the
        next `snapshot()`.
    */
    bool applyPatchSource (int slot,
                           const PatchSourceRequest& source,
                           std::string& error);

    //==========================================================================
    // Native plugin editor (T129)

    /** Opens or closes the patch's native editor window.

        The handler resolves the live hosted plugin, marshals to the message
        thread and returns a readable error when the window cannot be created.
        It runs on an HTTP thread, never the audio thread. Empty means no audio
        session is attached.
    */
    using PatchEditorHandler = std::function<bool (int slot, std::string& error)>;
    void setPatchEditorHandlers (PatchEditorHandler openEditor,
                                 PatchEditorHandler closeEditor);

    /** Applies `POST /patches/{slot}/editor`. */
    bool openPatchEditor (int slot, std::string& error);

    /** Applies `DELETE /patches/{slot}/editor`. Idempotent. */
    bool closePatchEditor (int slot, std::string& error);

    //==========================================================================
    // Explicit patch-state capture (T132)

    /** Saves the running plugin's current sound into its patch.

        The handler captures the resident instance's state and writes the
        patch's sidecar blob immediately. It runs on an HTTP thread, never the
        audio thread. Empty means no audio session is attached.
    */
    using PatchStateCaptureHandler =
        std::function<bool (int slot, PatchStateCapture& capture, std::string& error)>;
    void setPatchStateCaptureHandler (PatchStateCaptureHandler handler);

    /** Applies `POST /patches/{slot}/state/capture`. */
    bool capturePatchState (int slot,
                            PatchStateCapture& capture,
                            std::string& error);

    /** Applies a patch's effective linear gain to the audio graph.

        `gain` already includes the mute/solo decision, so the handler only has
        to publish it to the slot. It runs outside the store's lock, on the
        thread that made the change, and must be lock-free (never the audio
        thread).
    */
    using PatchHandler = std::function<void (int slot,
                                             const PatchState& patch,
                                             float gain)>;
    void setPatchHandler (PatchHandler handler);

    //==============================================================================
    // Per-patch insert chain (T121)

    /** Installs the live chain reader and editor. Called by `attachSession()`.

        Both run outside the store's lock. The handler must be safe to call
        from an HTTP thread and must not be called from the audio thread.
    */
    void setFxHandlers (FxChainProvider provider, FxCommandHandler handler);

    /** Reads one patch's chain. Returns false with a message for a bad slot or
        when no audio session is attached. */
    bool getFxChain (int slot, FxChainState& chain, std::string& error) const;

    /** Applies one chain mutation and returns the resulting chain. */
    bool applyFxCommand (int slot,
                         const FxCommand& command,
                         FxChainState& chain,
                         std::string& error);

    //==============================================================================
    // Inspection

    StateSnapshot snapshot() const;
    double getUptimeMs() const noexcept;

private:
    /** Persistent mixer/colour state of one slot (T120). */
    struct PatchMixState
    {
        double volumeDb = 0.0;
        double pan = 0.0;
        bool mute = false;
        bool solo = false;
        int colorIndex = 0;
    };

    PatchState makePatchLocked (int slot) const;
    void collectEffectiveGainsLocked (const std::vector<PatchMixState>& mixes,
                                      std::vector<float>& gains) const;

    mutable std::mutex mutex;

    const int numSlots;
    int armedSlot = -1;
    std::vector<std::string> slotNames;
    std::vector<std::string> slotFamilies;
    std::vector<std::string> slotFamilyNames;
    std::vector<std::string> slotPatches;
    std::vector<std::string> slotPatchNames;
    std::vector<int> slotLatencySamples;
    std::vector<double> slotLatencyMs;
    std::vector<PatchInstrumentState> slotInstruments;
    std::vector<PatchMixState> slotMixes;
    std::vector<bool> slotEditorOpen;
    std::vector<FamilyState> families;
    SettingsState settings;
    TransportState transport;

    ArmHandler armHandler;
    TransportHandler transportHandler;
    ChangeHandler changeHandler;
    PatchHandler patchHandler;
    PatchSourceHandler patchSourceHandler;
    PatchEditorHandler patchEditorHandler;
    PatchEditorHandler patchEditorCloseHandler;
    PatchStateCaptureHandler patchStateCaptureHandler;
    FxChainProvider fxProvider;
    FxCommandHandler fxHandler;
    TransportProvider transportProvider;
    SlotRuntimeProvider slotRuntimeProvider;

    const std::chrono::steady_clock::time_point startedAt;
};
} // namespace Api
