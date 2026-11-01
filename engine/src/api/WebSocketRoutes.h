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

class WebSocketRoutes
{
public:
    WebSocketRoutes (MeterSampler& meters,
                     StateBroadcast& broadcast,
                     const AuthGuard& auth,
                     const std::atomic<bool>& running);

    WebSocketRoutes (const WebSocketRoutes&) = delete;
    WebSocketRoutes& operator= (const WebSocketRoutes&) = delete;

    void registerRoutes (httplib::Server& server);

    std::uint64_t getMeterFrameCount() const noexcept;

    int getActiveConnections() const noexcept;

private:
    MeterSampler& meters;
    StateBroadcast& broadcast;
    const AuthGuard& auth;
    const std::atomic<bool>& running;

    std::atomic<int> activeConnections { 0 };
    std::atomic<std::uint64_t> meterFramesSent { 0 };
};

}
