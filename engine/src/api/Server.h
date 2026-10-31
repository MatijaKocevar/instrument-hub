#pragma once

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

//==============================================================================
// Control API state
//==============================================================================

/** Static description of one audio slot. */
struct SlotSnapshot
{
    int index = 0;
    std::string name;
    bool armed = false;
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

/** A consistent-enough snapshot of everything `GET /state` returns. */
struct StateSnapshot
{
    int schemaVersion = 1;
    std::string applicationName = "Instrument Hub";
    std::string applicationVersion = "0.1.0";

    int armedSlot = -1;
    std::vector<SlotSnapshot> slots;

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
    message thread. The store never touches the audio thread; a later task
    attaches `setArmHandler()` / `setTransportHandler()` to bridge requests
    into the lock-free graph controls.
*/
class StateStore
{
public:
    explicit StateStore (int numSlots = 8);

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

    using TransportHandler = std::function<void (const TransportState&)>;
    void setTransportHandler (TransportHandler handler);

    //==============================================================================
    // Slots

    int getNumSlots() const noexcept;
    void setSlotName (int slot, std::string name);

    //==============================================================================
    // Inspection

    StateSnapshot snapshot() const;
    double getUptimeMs() const noexcept;

private:
    mutable std::mutex mutex;

    const int numSlots;
    int armedSlot = -1;
    std::vector<std::string> slotNames;
    SettingsState settings;
    TransportState transport;

    ArmHandler armHandler;
    TransportHandler transportHandler;

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
    - `GET  /audio`
    - `POST /audio`
    - `POST /audio/measure`
    - `GET  /meters` (WebSocket upgrade)

    Binding defaults to `127.0.0.1:8080` (plan/DECISIONS.md). Authentication is
    off unless `Options::authToken` is non-empty; when enabled, requests must
    carry `X-Instrument-Hub-Token: <token>` or `Authorization: Bearer <token>`.

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

    /** Number of clients that received at least one meter frame. */
    std::uint64_t getMeterFrameCount() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace Api
