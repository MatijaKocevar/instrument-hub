#pragma once

#include <atomic>
#include <cstdint>

namespace httplib
{
class Server;
}

namespace Api
{

class AuthGuard;
class MeterSampler;
class StateBroadcast;

//==============================================================================
/**
    The two realtime WebSocket endpoints of the control API.

    - `/meters` streams the newest frame produced by `MeterSampler`, coalesced
      per connection: a slow client skips intermediate frames instead of
      accumulating a backlog.
    - `/events` hands the socket to `StateBroadcast`, which mirrors the live
      state document after every mutation.

    Both upgrades are authenticated through `AuthGuard` before the socket loop
    starts. The loops poll `running` so `Server::stop()` can let them exit; the
    active-connection count is what `Server` waits on before tearing down the
    listener.
*/
class WebSocketRoutes
{
public:
    WebSocketRoutes (MeterSampler& meters,
                     StateBroadcast& broadcast,
                     const AuthGuard& auth,
                     const std::atomic<bool>& running);

    WebSocketRoutes (const WebSocketRoutes&) = delete;
    WebSocketRoutes& operator= (const WebSocketRoutes&) = delete;

    /** Installs `/meters` and `/events` on `server`. */
    void registerRoutes (httplib::Server& server);

    /** Meter frames written to clients (one per successful send). */
    std::uint64_t getMeterFrameCount() const noexcept;

    /** WebSocket connections currently being served. */
    int getActiveConnections() const noexcept;

private:
    MeterSampler& meters;
    StateBroadcast& broadcast;
    const AuthGuard& auth;
    const std::atomic<bool>& running;

    std::atomic<int> activeConnections { 0 };
    std::atomic<std::uint64_t> meterFramesSent { 0 };
};

} // namespace Api
