#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

namespace httplib::ws
{
class WebSocket;
}

namespace Api
{

class StateStore;

//==============================================================================
// Live state mirror (T103)
//==============================================================================

/** What caused the latest state frame. */
enum class StateEventKind
{
    initial = 0, ///< the frame every client receives on connect
    pad,         ///< a hardware pad went down or up
    arm,         ///< the armed slot changed (`POST /arm`, pad selection)
    settings,    ///< `POST /settings`
    transport,   ///< `POST /transport`
    slot         ///< a slot name or latency changed
};

/** One mirrored pad. */
struct PadMirror
{
    bool pressed = false;
    /** `StateStore` uptime when the press/release arrived. */
    double updatedMs = 0.0;
};

/** Coalescing/polling knobs for the broadcast. */
struct StateBroadcastOptions
{
    /** How long a client waits in `read()` before it looks for a newer frame.

        A shorter interval lowers latency but wakes every client thread more
        often. The acceptance contract is 100 ms; 5 ms leaves plenty of slack
        for scheduling on a busy machine.
    */
    int clientPollIntervalUs = 5000;

    /** Maximum number of pads mirrored. The Launchkey Mini MK3 has 16. */
    static constexpr int maxPads = 16;
};

/**
    Fans the engine's live state out to every connected WebSocket client.

    The broadcaster owns the `/events` socket. A pad press and an HTTP state
    mutation both end in `publish...()`, which:

    - updates the mirrored pad state,
    - takes one `StateStore::snapshot()` (mutex protected, never the audio
      thread),
    - serialises one JSON frame,
    - bumps a version number clients poll.

    Clients never queue frames: each connection sends the newest frame it has
    not sent yet, so a slow client skips intermediate events instead of
    accumulating a backlog (the same coalescing rule as `/meters`). Publishing
    is cheap enough to happen per pad event; the coalescing happens at the
    socket.

    Threading: `publish...()` may be called from the MIDI input thread, an HTTP
    worker or the message thread. `handleClient()` runs on one WebSocket worker
    thread per connection and returns when the client closes or `stop()` is
    called. Nothing here is called from the audio callback.

    Only one broadcaster may be attached to a `StateStore`; the constructor
    installs the store's change handler and the destructor removes it.
*/
class StateBroadcast
{
public:
    explicit StateBroadcast (StateStore& state,
                             StateBroadcastOptions options = {});
    ~StateBroadcast();

    StateBroadcast (const StateBroadcast&) = delete;
    StateBroadcast& operator= (const StateBroadcast&) = delete;

    //==============================================================================
    // Lifecycle

    /** Starts accepting clients and publishes the initial frame. */
    void start();
    void stop();

    bool isRunning() const noexcept;

    //==============================================================================
    // Publishing

    /** Mirrors a hardware pad press/release and wakes every client.

        Out-of-range pads are ignored. The call returns after the new frame is
        visible to clients; it never waits for a socket to drain.
    */
    void publishPad (int pad, bool pressed);

    /** Publishes a state change caused by an HTTP/MIDI mutation.

        `kind` is one of `"arm"`, `"settings"`, `"transport"`, `"slot"` (or
        anything else for a generic state change). `slot` is only used by
        `"arm"`/`"slot"` events and may be negative.
    */
    void publishStateChange (const char* kind, int slot = -1);

    //==============================================================================
    // WebSocket connection

    /** Serves one `/events` connection until it closes or `stop()` is called.

        Blocks the calling worker thread. The caller has already authenticated
        the upgrade.
    */
    void handleClient (httplib::ws::WebSocket& websocket);

    //==============================================================================
    // Inspection / tests

    PadMirror getPad (int pad) const;

    /** Number of frames published (each `publish...()` call is one). */
    std::uint64_t getEventCount() const noexcept;
    /** Number of frames successfully written to clients. */
    std::uint64_t getFrameCount() const noexcept;
    /** Connections currently being served. */
    int getClientCount() const noexcept;
    /** Sequence number of the newest frame, 0 before `start()`. */
    std::uint64_t getLastSequence() const noexcept;

    /** The newest serialised frame. Empty before the first publish. */
    std::string getLastFrameJson() const;

private:
    void publish (StateEventKind kind, int pad, int slot, bool pressed);

    std::string buildFrameLocked (std::uint64_t sequence,
                                  StateEventKind kind,
                                  int pad,
                                  int slot,
                                  bool pressed,
                                  double timestampMs);

    StateStore& state;
    const StateBroadcastOptions options;

    std::atomic<bool> running { false };
    std::atomic<std::uint64_t> nextSequence { 0 };
    std::atomic<std::uint64_t> eventsPublished { 0 };
    std::atomic<std::uint64_t> framesSent { 0 };
    std::atomic<int> clients { 0 };

    mutable std::mutex frameMutex;
    std::array<PadMirror, StateBroadcastOptions::maxPads> pads {};
    std::string latestFrameJson;
    std::uint64_t latestSequence = 0;
};

} // namespace Api
