#include "Server.h"

#include "AuthGuard.h"
#include "MeterSampler.h"
#include "Routes.h"
#include "StateBroadcast.h"
#include "WebSocketRoutes.h"

#include "settings/SettingsStore.h"

#include <httplib.h>

#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>
#include <utility>

namespace Api
{

struct Server::Impl
{
    Impl (StateStore& storeToUse, Options optionsToUse)
        : state (storeToUse),
          options (std::move (optionsToUse)),
          pairing ({}, options.port),
          auth (pairing, options.authToken),
          broadcast (storeToUse),
          meters (storeToUse, options.meterHz),
          webSockets (meters, broadcast, auth, running)
    {
        nextPort = options.port;

        auto* store = state.getSettingsStore();

        if (store != nullptr)
        {
            const auto& account = store->settings().account;

            if (account.pairingToken.isNotEmpty())
                pairing.restoreToken (account.pairingToken.toStdString());

            if (options.lanMode || account.lanEnabled)
                pairing.setLanEnabled (true);

            pairing.setAccountChangeHandler ([this] (bool lan, const std::string& token)
            {
                state.persistAccountSettings (lan, token);
            });
        }
        else if (options.lanMode)
        {
            pairing.setLanEnabled (true);
        }
    }

    std::string desiredAddress() const
    {
        return pairing.isLanEnabled() ? "0.0.0.0" : options.address;
    }

    void startDiscoveryLocked()
    {
        DiscoveryService service;
        service.instanceName = "Instrument Hub";
        service.port = boundPort.load (std::memory_order_acquire);
        service.address = pairing.getAdvertisedAddress();

        if (service.address.rfind ("127.", 0) == 0)
            service.address.clear();

        if (! discovery.start (std::move (service)))
            std::cerr << "mdns: could not advertise " << Discovery::serviceType
                      << ": " << discovery.getLastError() << std::endl;
    }

    bool bindListenerLocked()
    {
        if (listening.load (std::memory_order_acquire))
            return true;

        http = std::make_unique<httplib::Server>();

        http->set_pre_routing_handler ([this] (const httplib::Request& request,
                                               httplib::Response& response)
        {
            const bool webSocketUpgrade = request.path == "/meters"
                                          || request.path == "/events";

            if (! webSocketUpgrade)
                requestsInFlight.fetch_add (1, std::memory_order_acq_rel);

            if (rebinding.load (std::memory_order_acquire))
            {
                if (! webSocketUpgrade)
                    requestsInFlight.fetch_sub (1, std::memory_order_acq_rel);

                setError (response, 503, "the control API is switching LAN mode");
                return httplib::Server::HandlerResponse::Handled;
            }

            if (request.method == "OPTIONS")
            {
                addCorsHeaders (response);
                response.status = 204;
                return httplib::Server::HandlerResponse::Handled;
            }

            if (! auth.authorized (request))
            {
                setError (response, 401, "unauthorized");
                return httplib::Server::HandlerResponse::Handled;
            }

            return httplib::Server::HandlerResponse::Unhandled;
        });

        http->set_post_routing_handler ([this] (const httplib::Request& request,
                                                httplib::Response&)
        {
            if (request.path != "/meters" && request.path != "/events")
                requestsInFlight.fetch_sub (1, std::memory_order_acq_rel);
        });

        RouteContext context { state, {}, {}, {}, {}, &pairing,
                               {}, {}, {}, {}, {}, {}, {}, {} };

        {
            const std::lock_guard<std::mutex> lock (configMutex);
            context.audioProvider = audioProvider;
            context.audioApply = audioApply;
            context.audioMeasure = audioMeasure;
            context.pluginProvider = pluginProvider;
            context.pluginRescan = pluginRescan;
            context.midiProvider = midiProvider;
            context.midiEnableInput = midiEnableInput;
            context.midiPadTest = midiPadTest;
            context.projectProvider = projectProvider;
            context.projectCommand = projectCommand;
        }

        context.setLanMode = [this] (bool enabled) { return applyLanMode (enabled); };

        registerRoutes (*http, context);
        webSockets.registerRoutes (*http);

        const auto address = desiredAddress();
        int port = nextPort;

        if (port > 0)
        {
            if (! http->bind_to_port (address, port))
            {
                http.reset();
                return false;
            }
        }
        else
        {
            port = http->bind_to_any_port (address);

            if (port <= 0)
            {
                http.reset();
                return false;
            }
        }

        boundPort.store (port, std::memory_order_release);
        nextPort = port;
        listenerAddress = address;
        listening.store (true, std::memory_order_release);
        pairing.setEndpoint ({}, port);

        if (pairing.isLanEnabled())
            startDiscoveryLocked();

        listenerThread = std::thread ([this] { http->listen_after_bind(); });

        return true;
    }

    void stopListenerLocked()
    {
        discovery.stop();

        if (! listening.exchange (false, std::memory_order_acq_rel))
        {
            if (http != nullptr)
                http.reset();

            return;
        }

        if (http != nullptr)
            http->stop();

        if (listenerThread.joinable())
            listenerThread.join();

        http.reset();
        boundPort.store (0, std::memory_order_release);
    }

    bool rebindLocked()
    {
        stopListenerLocked();
        return bindListenerLocked();
    }

    PairingSnapshot applyLanMode (bool enabled)
    {
        const auto changed = pairing.setLanEnabled (enabled);
        const auto snapshot = pairing.snapshot();

        if (changed)
        {
            {
                const std::lock_guard<std::mutex> lock (controlMutex);
                rebindRequested = true;
            }

            controlCv.notify_one();
        }

        return snapshot;
    }

    void waitForRequestsToDrain()
    {
        const auto deadline = std::chrono::steady_clock::now()
                              + std::chrono::seconds (2);

        while (requestsInFlight.load (std::memory_order_acquire) > 0
               && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for (std::chrono::milliseconds (5));
        }

        std::this_thread::sleep_for (std::chrono::milliseconds (100));
    }

    void controlLoop()
    {
        for (;;)
        {
            std::unique_lock<std::mutex> lock (controlMutex);
            controlCv.wait (lock, [this]
            {
                return controlStop || rebindRequested;
            });

            if (controlStop)
                return;

            rebindRequested = false;
            lock.unlock();

            rebinding.store (true, std::memory_order_release);
            waitForRequestsToDrain();

            {
                const std::lock_guard<std::mutex> life (lifecycleMutex);

                if (running.load (std::memory_order_acquire))
                    rebindLocked();
            }

            rebinding.store (false, std::memory_order_release);
        }
    }

    StateStore& state;
    const Options options;

    std::atomic<bool> running { false };
    std::atomic<bool> listening { false };
    std::atomic<bool> rebinding { false };
    std::atomic<int> requestsInFlight { 0 };
    std::atomic<int> boundPort { 0 };

    mutable std::mutex configMutex;
    AudioStateProvider audioProvider;
    AudioApplyHandler audioApply;
    AudioMeasureHandler audioMeasure;
    PluginInventoryProvider pluginProvider;
    PluginRescanHandler pluginRescan;
    MidiStateProvider midiProvider;
    MidiInputHandler midiEnableInput;
    MidiPadTestHandler midiPadTest;
    ProjectOverviewProvider projectProvider;
    ProjectCommandHandler projectCommand;

    PairingManager pairing;
    Discovery discovery;

    AuthGuard auth;
    StateBroadcast broadcast;
    MeterSampler meters;
    WebSocketRoutes webSockets;

    std::unique_ptr<httplib::Server> http;
    std::thread listenerThread;
    std::thread controlThread;

    mutable std::mutex lifecycleMutex;
    std::string listenerAddress;
    int nextPort = 0;

    std::mutex controlMutex;
    std::condition_variable controlCv;
    bool controlStop = false;
    bool rebindRequested = false;
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
    const std::lock_guard<std::mutex> life (self.lifecycleMutex);

    if (self.running.load (std::memory_order_acquire))
        return true;

    if (! self.bindListenerLocked())
        return false;

    self.running.store (true, std::memory_order_release);
    self.broadcast.start();
    self.meters.start();

    {
        const std::lock_guard<std::mutex> lock (self.controlMutex);
        self.controlStop = false;
        self.rebindRequested = false;
    }

    self.controlThread = std::thread ([&self] { self.controlLoop(); });

    return true;
}

void Server::stop()
{
    auto& self = *impl;

    {
        const std::lock_guard<std::mutex> lock (self.controlMutex);
        self.controlStop = true;
        self.rebindRequested = false;
    }

    self.controlCv.notify_all();

    self.broadcast.stop();

    if (self.controlThread.joinable())
        self.controlThread.join();

    {
        const std::lock_guard<std::mutex> life (self.lifecycleMutex);
        self.running.store (false, std::memory_order_release);
        self.stopListenerLocked();
    }

    for (int spin = 0; spin < 200
                       && self.webSockets.getActiveConnections() > 0;
         ++spin)
    {
        std::this_thread::sleep_for (std::chrono::milliseconds (10));
    }

    self.meters.stop();

    self.boundPort.store (0, std::memory_order_release);
}

bool Server::isRunning() const noexcept
{
    return impl->running.load (std::memory_order_acquire);
}

bool Server::isListening() const noexcept
{
    return impl->listening.load (std::memory_order_acquire);
}

int Server::getPort() const noexcept
{
    return impl->boundPort.load (std::memory_order_acquire);
}

std::string Server::getAddress() const
{
    const std::lock_guard<std::mutex> life (impl->lifecycleMutex);
    return impl->listenerAddress.empty() ? impl->options.address : impl->listenerAddress;
}

void Server::setMeterProvider (MeterProvider provider)
{
    impl->meters.setProvider (std::move (provider));
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

void Server::setPluginHandlers (PluginInventoryProvider provider,
                                PluginRescanHandler rescan)
{
    const std::lock_guard<std::mutex> lock (impl->configMutex);
    impl->pluginProvider = std::move (provider);
    impl->pluginRescan = std::move (rescan);
}

void Server::setMidiHandlers (MidiStateProvider provider,
                              MidiInputHandler enableInput,
                              MidiPadTestHandler padTest)
{
    const std::lock_guard<std::mutex> lock (impl->configMutex);
    impl->midiProvider = std::move (provider);
    impl->midiEnableInput = std::move (enableInput);
    impl->midiPadTest = std::move (padTest);
}

void Server::setProjectHandlers (ProjectOverviewProvider provider,
                                 ProjectCommandHandler command)
{
    const std::lock_guard<std::mutex> lock (impl->configMutex);
    impl->projectProvider = std::move (provider);
    impl->projectCommand = std::move (command);
}

void Server::setAuthToken (std::string token)
{
    impl->auth.setToken (std::move (token));
}

bool Server::hasAuthToken() const
{
    return impl->auth.hasToken();
}

PairingManager& Server::getPairing()
{
    return impl->pairing;
}

const PairingManager& Server::getPairing() const
{
    return impl->pairing;
}

Discovery& Server::getDiscovery()
{
    return impl->discovery;
}

const Discovery& Server::getDiscovery() const
{
    return impl->discovery;
}

StateBroadcast& Server::getStateBroadcast()
{
    return impl->broadcast;
}

const StateBroadcast& Server::getStateBroadcast() const
{
    return impl->broadcast;
}

PairingSnapshot Server::setLanMode (bool enabled)
{
    return impl->applyLanMode (enabled);
}

bool Server::isLanModeEnabled() const
{
    return impl->pairing.isLanEnabled();
}

std::uint64_t Server::getMeterFrameCount() const noexcept
{
    return impl->webSockets.getMeterFrameCount();
}

}
