#include "WebSocketRoutes.h"

#include "AuthGuard.h"
#include "MeterSampler.h"
#include "StateBroadcast.h"

#include <httplib.h>

#include <cstdint>
#include <string>

namespace Api
{

WebSocketRoutes::WebSocketRoutes (MeterSampler& metersToUse,
                                  StateBroadcast& broadcastToUse,
                                  const AuthGuard& authToUse,
                                  const std::atomic<bool>& runningToUse)
    : meters (metersToUse),
      broadcast (broadcastToUse),
      auth (authToUse),
      running (runningToUse)
{
}

void WebSocketRoutes::registerRoutes (httplib::Server& server)
{
    server.WebSocket ("/meters", [this] (const httplib::Request& request,
                                          httplib::ws::WebSocket& websocket)
    {
        if (! auth.authorized (request))
        {
            websocket.close (httplib::ws::CloseStatus::PolicyViolation, "unauthorized");
            return;
        }

        activeConnections.fetch_add (1, std::memory_order_acq_rel);

        websocket.set_read_timeout (0, 10000);

        std::uint64_t sentVersion = 0;
        std::string incoming;

        while (running.load (std::memory_order_acquire) && websocket.is_open())
        {
            const auto frame = meters.getLatestFrame();

            if (frame.version != 0 && frame.version != sentVersion)
            {
                if (! websocket.send (frame.json))
                    break;

                sentVersion = frame.version;
                meterFramesSent.fetch_add (1, std::memory_order_relaxed);
            }

            const auto result = websocket.read (incoming);

            if (result == httplib::ws::ReadResult::Fail)
                break;
        }

        if (websocket.is_open())
            websocket.close (httplib::ws::CloseStatus::Normal, "server stopping");

        activeConnections.fetch_sub (1, std::memory_order_acq_rel);
    });

    server.WebSocket ("/events", [this] (const httplib::Request& request,
                                          httplib::ws::WebSocket& websocket)
    {
        if (! auth.authorized (request))
        {
            websocket.close (httplib::ws::CloseStatus::PolicyViolation, "unauthorized");
            return;
        }

        activeConnections.fetch_add (1, std::memory_order_acq_rel);
        broadcast.handleClient (websocket);
        activeConnections.fetch_sub (1, std::memory_order_acq_rel);
    });
}

std::uint64_t WebSocketRoutes::getMeterFrameCount() const noexcept
{
    return meterFramesSent.load (std::memory_order_acquire);
}

int WebSocketRoutes::getActiveConnections() const noexcept
{
    return activeConnections.load (std::memory_order_acquire);
}

}
