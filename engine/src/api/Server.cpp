#include "Server.h"
#include "Routes.h"

#include <httplib.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <thread>
#include <utility>

namespace Api
{

namespace
{
constexpr int minimumMeterHz = 1;
constexpr int maximumMeterHz = 120;

int clampMeterHz (int hz) noexcept
{
    return juce::jlimit (minimumMeterHz, maximumMeterHz, hz);
}
} // namespace

//==============================================================================
// StateStore
//==============================================================================

StateStore::StateStore (int numSlotsToUse)
    : numSlots (juce::jmax (1, numSlotsToUse)),
      startedAt (std::chrono::steady_clock::now())
{
    slotNames.reserve (static_cast<std::size_t> (numSlots));

    for (int index = 0; index < numSlots; ++index)
        slotNames.push_back ("Slot " + std::to_string (index + 1));
}

int StateStore::getArmedSlot() const noexcept
{
    const std::lock_guard<std::mutex> lock (mutex);
    return armedSlot;
}

bool StateStore::setArmedSlot (int slot)
{
    ArmHandler handler;
    int appliedSlot = -1;

    {
        const std::lock_guard<std::mutex> lock (mutex);

        if (slot >= numSlots)
            return false;

        appliedSlot = slot < 0 ? -1 : slot;
        armedSlot = appliedSlot;
        handler = armHandler;
    }

    if (handler != nullptr)
        handler (appliedSlot);

    return true;
}

void StateStore::setArmHandler (ArmHandler handler)
{
    const std::lock_guard<std::mutex> lock (mutex);
    armHandler = std::move (handler);
}

SettingsState StateStore::getSettings() const
{
    const std::lock_guard<std::mutex> lock (mutex);
    return settings;
}

bool StateStore::updateSettings (const juce::var& patch, std::string& error)
{
    if (! patch.isObject())
    {
        error = "request body must be a JSON object";
        return false;
    }

    const std::lock_guard<std::mutex> lock (mutex);
    auto next = settings;
    const auto& properties = patch.getDynamicObject()->getProperties();

    for (int index = 0; index < properties.size(); ++index)
    {
        const auto& name = properties.getName (index);
        const auto& value = properties.getValueAt (index);

        if (name.toString() == "masterGain")
        {
            double parsed = 0.0;

            if (! detail::readNumber (value, "masterGain", 0.0, 1.5, parsed, error))
                return false;

            next.masterGain = parsed;
        }
        else if (name.toString() == "inputGain")
        {
            double parsed = 0.0;

            if (! detail::readNumber (value, "inputGain", 0.0, 2.0, parsed, error))
                return false;

            next.inputGain = parsed;
        }
        else if (name.toString() == "midiChannel")
        {
            std::int64_t parsed = 0;

            if (! detail::readInteger (value, "midiChannel", 1, 16, parsed, error))
                return false;

            next.midiChannel = static_cast<int> (parsed);
        }
        else if (name.toString() == "metronome")
        {
            bool parsed = false;

            if (! detail::readBool (value, "metronome", parsed, error))
                return false;

            next.metronome = parsed;
        }
        else
        {
            error = "unknown setting: " + name.toString().toStdString();
            return false;
        }
    }

    settings = next;
    return true;
}

TransportState StateStore::getTransport() const
{
    const std::lock_guard<std::mutex> lock (mutex);
    return transport;
}

bool StateStore::updateTransport (const juce::var& patch, std::string& error)
{
    if (! patch.isObject())
    {
        error = "request body must be a JSON object";
        return false;
    }

    TransportHandler handler;
    TransportState next;

    {
        const std::lock_guard<std::mutex> lock (mutex);
        next = transport;
        const auto& properties = patch.getDynamicObject()->getProperties();

        for (int index = 0; index < properties.size(); ++index)
        {
            const auto& name = properties.getName (index);
            const auto& value = properties.getValueAt (index);

            if (name.toString() == "playing")
            {
                if (! detail::readBool (value, "playing", next.playing, error))
                    return false;
            }
            else if (name.toString() == "bpm")
            {
                double parsed = 0.0;

                if (! detail::readNumber (value, "bpm", 20.0, 300.0, parsed, error))
                    return false;

                next.bpm = parsed;
            }
            else if (name.toString() == "ppq")
            {
                double parsed = 0.0;

                if (! detail::readNumber (value, "ppq", 0.0, 1.0e9, parsed, error))
                    return false;

                next.ppq = parsed;
            }
            else if (name.toString() == "positionSamples")
            {
                std::int64_t parsed = 0;

                if (! detail::readInteger (value, "positionSamples",
                                   0, std::numeric_limits<std::int64_t>::max(),
                                   parsed, error))
                {
                    return false;
                }

                next.positionSamples = parsed;
            }
            else
            {
                error = "unknown transport field: " + name.toString().toStdString();
                return false;
            }
        }

        transport = next;
        handler = transportHandler;
    }

    if (handler != nullptr)
        handler (next);

    return true;
}

void StateStore::setTransportHandler (TransportHandler handler)
{
    const std::lock_guard<std::mutex> lock (mutex);
    transportHandler = std::move (handler);
}

int StateStore::getNumSlots() const noexcept
{
    return numSlots;
}

void StateStore::setSlotName (int slot, std::string name)
{
    const std::lock_guard<std::mutex> lock (mutex);

    if (slot < 0 || slot >= numSlots)
        return;

    slotNames[static_cast<std::size_t> (slot)] = std::move (name);
}

StateSnapshot StateStore::snapshot() const
{
    StateSnapshot result;

    {
        const std::lock_guard<std::mutex> lock (mutex);

        result.armedSlot = armedSlot;
        result.settings = settings;
        result.transport = transport;
        result.slots.reserve (static_cast<std::size_t> (numSlots));

        for (int index = 0; index < numSlots; ++index)
        {
            SlotSnapshot slot;
            slot.index = index;
            slot.name = slotNames[static_cast<std::size_t> (index)];
            slot.armed = index == armedSlot;
            result.slots.push_back (std::move (slot));
        }
    }

    result.uptimeMs = getUptimeMs();
    return result;
}

double StateStore::getUptimeMs() const noexcept
{
    const auto elapsed = std::chrono::steady_clock::now() - startedAt;
    return std::chrono::duration<double, std::milli> (elapsed).count();
}

//==============================================================================
// Server
//==============================================================================

struct Server::Impl
{
    Impl (StateStore& storeToUse, Options optionsToUse)
        : state (storeToUse),
          options (std::move (optionsToUse)),
          authToken (options.authToken),
          meterHz (clampMeterHz (options.meterHz))
    {
    }

    bool authorized (const httplib::Request& request) const
    {
        std::string expected;

        {
            const std::lock_guard<std::mutex> lock (configMutex);
            expected = authToken;
        }

        if (expected.empty())
            return true;

        auto supplied = request.get_header_value ("X-Instrument-Hub-Token");

        if (supplied.empty())
        {
            const auto authorization = request.get_header_value ("Authorization");
            constexpr const char* bearer = "Bearer ";

            if (authorization.rfind (bearer, 0) == 0)
                supplied = authorization.substr (std::char_traits<char>::length (bearer));
        }

        return supplied == expected;
    }

    MeterFrame makeDefaultFrame() const
    {
        const auto snapshot = state.snapshot();

        MeterFrame frame;
        frame.timestampMs = snapshot.uptimeMs;
        frame.slots.reserve (snapshot.slots.size());

        for (const auto& slot : snapshot.slots)
        {
            SlotMeter meter;
            meter.slot = slot.index;
            meter.armed = slot.armed;
            meter.gain = slot.armed ? 1.0f : 0.0f;
            frame.slots.push_back (meter);
        }

        return frame;
    }

    void meterLoop()
    {
        const auto interval = std::chrono::milliseconds (1000 / meterHz);
        auto next = std::chrono::steady_clock::now();
        std::uint64_t sequence = 0;

        while (running.load (std::memory_order_acquire))
        {
            MeterProvider provider;

            {
                const std::lock_guard<std::mutex> lock (configMutex);
                provider = meterProvider;
            }

            MeterFrame frame;

            if (provider != nullptr)
            {
                try
                {
                    frame = provider();
                }
                catch (...)
                {
                    frame = makeDefaultFrame();
                }
            }
            else
            {
                frame = makeDefaultFrame();
            }

            frame.sequence = ++sequence;

            if (! (frame.timestampMs > 0.0))
                frame.timestampMs = state.getUptimeMs();

            auto json = toJson (meterFrameToVar (frame));

            {
                const std::lock_guard<std::mutex> lock (frameMutex);
                latestFrameJson = std::move (json);
                latestFrameVersion = sequence;
            }

            next += interval;
            const auto now = std::chrono::steady_clock::now();

            if (next > now)
                std::this_thread::sleep_until (next);
            else
                next = now;
        }
    }

    void registerMeterRoute()
    {
        http->WebSocket ("/meters", [this] (const httplib::Request& request,
                                            httplib::ws::WebSocket& websocket)
        {
            if (! authorized (request))
            {
                websocket.close (httplib::ws::CloseStatus::PolicyViolation, "unauthorized");
                return;
            }

            activeWebSockets.fetch_add (1, std::memory_order_acq_rel);

            // Short read timeout: the loop must stay responsive to new meter
            // frames while still reading so ping/pong and closes are handled.
            websocket.set_read_timeout (0, 10000);

            std::uint64_t sentVersion = 0;
            std::string incoming;

            while (running.load (std::memory_order_acquire) && websocket.is_open())
            {
                std::string payload;
                std::uint64_t version = 0;

                {
                    const std::lock_guard<std::mutex> lock (frameMutex);
                    version = latestFrameVersion;

                    if (version != sentVersion)
                        payload = latestFrameJson;
                }

                if (version != sentVersion)
                {
                    if (! websocket.send (payload))
                        break;

                    sentVersion = version;
                    meterFramesSent.fetch_add (1, std::memory_order_relaxed);
                }

                const auto result = websocket.read (incoming);

                if (result == httplib::ws::ReadResult::Fail)
                    break;
            }

            if (websocket.is_open())
                websocket.close (httplib::ws::CloseStatus::Normal, "server stopping");

            activeWebSockets.fetch_sub (1, std::memory_order_acq_rel);
        });
    }

    StateStore& state;
    const Options options;

    std::unique_ptr<httplib::Server> http;
    std::thread listenerThread;
    std::thread meterThread;

    std::atomic<bool> running { false };
    std::atomic<int> boundPort { 0 };
    std::atomic<int> activeWebSockets { 0 };
    std::atomic<std::uint64_t> meterFramesSent { 0 };

    mutable std::mutex configMutex;
    MeterProvider meterProvider;
    AudioStateProvider audioProvider;
    AudioApplyHandler audioApply;
    AudioMeasureHandler audioMeasure;
    std::string authToken;
    const int meterHz;

    mutable std::mutex frameMutex;
    std::string latestFrameJson;
    std::uint64_t latestFrameVersion = 0;
};

Server::Server (StateStore& state)
    : Server (state, Options {})
{
}

Server::Server (StateStore& state, Options options)
    : impl (std::make_unique<Impl> (state, std::move (options)))
{
}

Server::~Server()
{
    stop();
}

bool Server::start()
{
    auto& self = *impl;

    if (self.running.load (std::memory_order_acquire))
        return true;

    self.http = std::make_unique<httplib::Server>();

    self.http->set_pre_routing_handler ([&self] (const httplib::Request& request,
                                                 httplib::Response& response)
    {
        // Route handlers add the CORS headers through setJson()/setError();
        // only a pre-flight response needs them before a handler runs.
        if (request.method == "OPTIONS")
        {
            addCorsHeaders (response);
            response.status = 204;
            return httplib::Server::HandlerResponse::Handled;
        }

        if (! self.authorized (request))
        {
            setError (response, 401, "unauthorized");
            return httplib::Server::HandlerResponse::Handled;
        }

        return httplib::Server::HandlerResponse::Unhandled;
    });

    RouteContext context { self.state, {}, {}, {}, {} };

    {
        const std::lock_guard<std::mutex> lock (self.configMutex);
        context.audioProvider = self.audioProvider;
        context.audioApply = self.audioApply;
        context.audioMeasure = self.audioMeasure;
    }

    registerRoutes (*self.http, context);
    self.registerMeterRoute();

    int port = self.options.port;

    if (port == 0)
    {
        port = self.http->bind_to_any_port (self.options.address);

        if (port <= 0)
        {
            self.http.reset();
            return false;
        }
    }
    else if (! self.http->bind_to_port (self.options.address, port))
    {
        self.http.reset();
        return false;
    }

    self.boundPort.store (port, std::memory_order_release);
    self.running.store (true, std::memory_order_release);

    self.meterThread = std::thread ([&self] { self.meterLoop(); });
    self.listenerThread = std::thread ([&self]
    {
        self.http->listen_after_bind();
    });

    return true;
}

void Server::stop()
{
    auto& self = *impl;

    if (! self.running.exchange (false, std::memory_order_acq_rel))
    {
        if (self.http != nullptr)
            self.http.reset();

        return;
    }

    if (self.http != nullptr)
        self.http->stop();

    if (self.listenerThread.joinable())
        self.listenerThread.join();

    // WebSocket handlers leave their read loop within one short timeout.
    for (int spin = 0; spin < 200
                       && self.activeWebSockets.load (std::memory_order_acquire) > 0;
         ++spin)
    {
        std::this_thread::sleep_for (std::chrono::milliseconds (10));
    }

    if (self.meterThread.joinable())
        self.meterThread.join();

    self.http.reset();
    self.boundPort.store (0, std::memory_order_release);
}

bool Server::isRunning() const noexcept
{
    return impl->running.load (std::memory_order_acquire);
}

bool Server::isListening() const noexcept
{
    return isRunning();
}

int Server::getPort() const noexcept
{
    return impl->boundPort.load (std::memory_order_acquire);
}

std::string Server::getAddress() const
{
    return impl->options.address;
}

void Server::setMeterProvider (MeterProvider provider)
{
    const std::lock_guard<std::mutex> lock (impl->configMutex);
    impl->meterProvider = std::move (provider);
}

void Server::setAudioHandlers (AudioStateProvider provider,
                               AudioApplyHandler apply,
                               AudioMeasureHandler measure)
{
    const std::lock_guard<std::mutex> lock (impl->configMutex);
    impl->audioProvider = std::move (provider);
    impl->audioApply = std::move (apply);
    impl->audioMeasure = std::move (measure);
}

void Server::setAuthToken (std::string token)
{
    const std::lock_guard<std::mutex> lock (impl->configMutex);
    impl->authToken = std::move (token);
}

bool Server::hasAuthToken() const
{
    const std::lock_guard<std::mutex> lock (impl->configMutex);
    return ! impl->authToken.empty();
}

std::uint64_t Server::getMeterFrameCount() const noexcept
{
    return impl->meterFramesSent.load (std::memory_order_acquire);
}

} // namespace Api
