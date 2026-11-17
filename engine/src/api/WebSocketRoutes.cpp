#include "WebSocketRoutes.h"

#include "AuthGuard.h"
#include "MeterSampler.h"
#include "StateBroadcast.h"

#include <httplib.h>
#include <juce_core/juce_core.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <utility>

namespace Api {
namespace {

bool isClientId(const std::string& value) {
    if (value.empty() || value.size() > 64) return false;

    for (const auto character : value) {
        const bool valid =
            (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') || character == '-' || character == '_';

        if (!valid) return false;
    }

    return true;
}

std::string cleanName(const juce::String& value) {
    auto name = value.trim();

    if (name.isEmpty()) name = "Phone MIDI";

    return name.substring(0, 48).toStdString();
}

bool parseHello(const std::string& message, std::string& clientId, std::string& name) {
    if (message.empty() || message.front() != '{') return false;

    const auto parsed = juce::JSON::parse(juce::String(message));
    auto* object = parsed.getDynamicObject();

    if (object == nullptr) return false;

    const auto id = object->getProperty("id").toString().toStdString();

    if (!isClientId(id)) return false;

    clientId = id;
    name = cleanName(object->getProperty("name").toString());

    return true;
}

bool isControlType(const std::string& message, const char* type) {
    if (message.empty() || message.front() != '{') return false;

    const auto parsed = juce::JSON::parse(juce::String(message));
    auto* object = parsed.getDynamicObject();

    if (object == nullptr) return false;

    return object->getProperty("type").toString() == juce::String(type);
}

}  // namespace

WebSocketRoutes::WebSocketRoutes(MeterSampler& metersToUse, StateBroadcast& broadcastToUse,
                                 const AuthGuard& authToUse, const std::atomic<bool>& runningToUse)
    : meters(metersToUse), broadcast(broadcastToUse), auth(authToUse), running(runningToUse) {}

void WebSocketRoutes::setNetworkMidiHandlers(NetworkMidiHandlers handlers) {
    networkMidi = std::move(handlers);
}

void WebSocketRoutes::registerRoutes(httplib::Server& server) {
    server.WebSocket(
        "/meters", [this](const httplib::Request& request, httplib::ws::WebSocket& websocket) {
            if (!auth.authorized(request)) {
                websocket.close(httplib::ws::CloseStatus::PolicyViolation, "unauthorized");
                return;
            }

            activeConnections.fetch_add(1, std::memory_order_acq_rel);

            websocket.set_read_timeout(0, 10000);

            std::uint64_t sentVersion = 0;
            std::string incoming;

            while (running.load(std::memory_order_acquire) && websocket.is_open()) {
                const auto frame = meters.getLatestFrame();

                if (frame.version != 0 && frame.version != sentVersion) {
                    if (!websocket.send(frame.json)) break;

                    sentVersion = frame.version;
                    meterFramesSent.fetch_add(1, std::memory_order_relaxed);
                }

                const auto result = websocket.read(incoming);

                if (result == httplib::ws::ReadResult::Fail) break;
            }

            if (websocket.is_open())
                websocket.close(httplib::ws::CloseStatus::Normal, "server stopping");

            activeConnections.fetch_sub(1, std::memory_order_acq_rel);
        });

    server.WebSocket(
        "/events", [this](const httplib::Request& request, httplib::ws::WebSocket& websocket) {
            if (!auth.authorized(request)) {
                websocket.close(httplib::ws::CloseStatus::PolicyViolation, "unauthorized");
                return;
            }

            activeConnections.fetch_add(1, std::memory_order_acq_rel);
            broadcast.handleClient(websocket);
            activeConnections.fetch_sub(1, std::memory_order_acq_rel);
        });

    server.WebSocket("/midi-input", [this](const httplib::Request& request,
                                           httplib::ws::WebSocket& websocket) {
        if (!auth.authorized(request)) {
            websocket.close(httplib::ws::CloseStatus::PolicyViolation, "unauthorized");
            return;
        }

        if (!networkMidi.attach || !networkMidi.detach || !networkMidi.midi) {
            websocket.close(httplib::ws::CloseStatus::InternalError, "network MIDI is unavailable");
            return;
        }

        activeConnections.fetch_add(1, std::memory_order_acq_rel);
        websocket.set_read_timeout(0, 5000);

        const auto session = nextSession.fetch_add(1, std::memory_order_acq_rel);
        std::string clientId;
        std::string name;
        bool attached = false;
        auto lastSeen = std::chrono::steady_clock::now();
        std::string incoming;

        while (running.load(std::memory_order_acquire) && websocket.is_open()) {
            const auto result = websocket.read(incoming);

            if (result == httplib::ws::ReadResult::Fail) break;

            const auto now = std::chrono::steady_clock::now();

            if (result == httplib::ws::ReadResult::Timeout) {
                if (attached && now - lastSeen > std::chrono::seconds(20)) break;

                continue;
            }

            lastSeen = now;

            if (!attached) {
                if (!parseHello(incoming, clientId, name)) {
                    websocket.close(httplib::ws::CloseStatus::PolicyViolation, "hello required");
                    break;
                }

                networkMidi.attach(clientId, name, session);
                attached = true;

                if (!websocket.send(R"({"type":"ready"})")) break;

                continue;
            }

            if (incoming.size() >= 2 && static_cast<unsigned char>(incoming.front()) == 1) {
                const auto* data = reinterpret_cast<const std::uint8_t*>(incoming.data()) + 1;
                const auto size = static_cast<int>(incoming.size()) - 1;

                if (size <= 3) (void)networkMidi.midi(clientId, session, data, size);

                continue;
            }

            if (isControlType(incoming, "ping")) {
                if (!websocket.send(R"({"type":"pong"})")) break;

                continue;
            }

            if (isControlType(incoming, "bye")) break;
        }

        if (attached) networkMidi.detach(clientId, session);

        if (websocket.is_open()) websocket.close(httplib::ws::CloseStatus::Normal, "closing");

        activeConnections.fetch_sub(1, std::memory_order_acq_rel);
    });
}

std::uint64_t WebSocketRoutes::getMeterFrameCount() const noexcept {
    return meterFramesSent.load(std::memory_order_acquire);
}

int WebSocketRoutes::getActiveConnections() const noexcept {
    return activeConnections.load(std::memory_order_acquire);
}

}  // namespace Api
