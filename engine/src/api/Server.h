#pragma once

#include "Discovery.h"
#include "Pairing.h"

#include <juce_core/juce_core.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace Api
{

class StateBroadcast;

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
};

/** Instrument descriptor published with every patch in the family tree.

    The descriptor is read-only over the control API: changing the source is
    the instrument page's job. It is present for every patch, hot or cold, so
    the grid can show a plugin/sampler patch before its instance is loaded.
*/
struct PatchInstrumentState
{
    std::string id;
    std::string name;
    std::string kind = "init-tone";
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
    std::vector<FamilyState> families;
    SettingsState settings;
    TransportState transport;

    ArmHandler armHandler;
    TransportHandler transportHandler;
    ChangeHandler changeHandler;
    PatchHandler patchHandler;
    TransportProvider transportProvider;
    SlotRuntimeProvider slotRuntimeProvider;

    const std::chrono::steady_clock::time_point startedAt;
};

//==============================================================================
// HTTP + WebSocket server
//==============================================================================

/**
    The local control server: one `cpp-httplib` HTTP listener plus a WebSocket
    endpoint for meters.

    Endpoints:

    - `GET  /state`
    - `POST /arm`
    - `GET  /settings`
    - `POST /settings`
    - `POST /transport`
    - `PATCH /patches/{slot}`
    - `GET  /audio`
    - `POST /audio`
    - `POST /audio/measure`
    - `GET  /meters` (WebSocket upgrade)
    - `GET  /events` (WebSocket upgrade, live state mirror)
    - `GET  /pairing` (loopback only)
    - `POST /pairing/lan` (loopback only)
    - `POST /pairing/revoke` (loopback only)
    - `POST /pair`

    Binding defaults to `127.0.0.1:8080` (plan/DECISIONS.md). Authentication is
    off unless `Options::authToken` is non-empty or LAN mode is enabled; when
    enabled, requests must carry `X-Instrument-Hub-Token: <token>` or
    `Authorization: Bearer <token>`. A pairing token obtained from `POST /pair`
    is accepted alongside the configured token.

    LAN mode is off by default. Enabling it rebinds the listener to all
    interfaces, makes authentication mandatory and starts a pairing code. The
    QR payload and code are local-only management endpoints; the phone/tablet
    only needs `POST /pair`.

    Meter frames are produced once per `meterHz` on a dedicated thread and
    coalesced for every connected client. The audio buffer rate is irrelevant.
*/
class Server
{
public:
    struct Options
    {
        std::string address = "127.0.0.1";
        int port = 8080;

        /** Empty disables authentication. */
        std::string authToken;

        /** Start with LAN mode already enabled (rebinds to all interfaces). */
        bool lanMode = false;

        /** Broadcast rate for `/meters`; clamped to 1..120. */
        int meterHz = 30;
    };

    explicit Server (StateStore& state);
    Server (StateStore& state, Options options);
    ~Server();

    Server (const Server&) = delete;
    Server& operator= (const Server&) = delete;

    //==============================================================================
    // Lifecycle

    /** Binds and starts the listener and the meter thread.

        Returns false when the address/port cannot be bound (for example when
        another instance already owns it).
    */
    bool start();
    void stop();

    bool isRunning() const noexcept;
    bool isListening() const noexcept;

    /** The actual bound port. Useful when `Options::port` is 0. */
    int getPort() const noexcept;
    std::string getAddress() const;

    //==============================================================================
    // Configuration

    /** Replaces the meter source. Safe before or after `start()`. */
    void setMeterProvider (MeterProvider provider);

    /** Installs the audio device backend behind `/audio` and `/audio/measure`.

        Call before `start()`; the handlers are copied into the route table.
        A missing provider makes `GET /audio` report a closed device, and the
        mutating endpoints answer `503`.
    */
    void setAudioHandlers (AudioStateProvider provider,
                           AudioApplyHandler apply,
                           AudioMeasureHandler measure);

    /** Enables/disables token auth. Takes effect on the next request. */
    void setAuthToken (std::string token);
    bool hasAuthToken() const;

    //==============================================================================
    // LAN pairing (T101)

    /** The pairing manager behind `/pairing` and `/pair`. */
    PairingManager& getPairing();
    const PairingManager& getPairing() const;

    /** The mDNS responder that advertises `_instrumenthub._tcp` in LAN mode
        (T102). It runs only while the listener is bound to all interfaces.
    */
    Discovery& getDiscovery();
    const Discovery& getDiscovery() const;

    /** The live state mirror behind the `/events` WebSocket (T103).

        A pad press, `POST /arm`, `POST /settings` or `POST /transport` all
        publish a frame; every connected client receives the newest state
        within a few milliseconds. The broadcaster starts and stops with the
        server.
    */
    StateBroadcast& getStateBroadcast();
    const StateBroadcast& getStateBroadcast() const;

    /** Turn LAN mode on/off.

        Enabling rebinds the listener from loopback to all interfaces, makes
        token auth mandatory and issues a pairing code. Disabling reverses both
        and revokes every issued token. Returns the state immediately after the
        toggle; the rebind itself happens on the server's control thread so the
        HTTP request that triggered it can finish.
    */
    PairingSnapshot setLanMode (bool enabled);
    bool isLanModeEnabled() const;

    /** Number of clients that received at least one meter frame. */
    std::uint64_t getMeterFrameCount() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace Api
