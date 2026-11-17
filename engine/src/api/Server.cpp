#include "Server.h"

#include "AuthGuard.h"
#include "MeterSampler.h"
#include "Routes.h"
#include "StateBroadcast.h"
#include "WebSocketRoutes.h"
#include "api/tls/TlsCertificate.h"

#include "settings/SettingsStore.h"

#include <httplib.h>
#include <juce_core/juce_core.h>

#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace Api {

struct Server::Impl {
    Impl(StateStore& storeToUse, Options optionsToUse)
        : state(storeToUse),
          options(std::move(optionsToUse)),
          pairing({}, options.port),
          auth(pairing, options.authToken),
          broadcast(storeToUse),
          meters(storeToUse, options.meterHz),
          webSockets(meters, broadcast, auth, running) {
        nextPort = options.port;

        auto* store = state.getSettingsStore();

        if (store != nullptr) {
            const auto& account = store->settings().account;

            if (account.pairingToken.isNotEmpty())
                pairing.restoreToken(account.pairingToken.toStdString());

            if (options.lanMode || account.lanEnabled) pairing.setLanEnabled(true);

            pairing.setAccountChangeHandler([this](bool lan, const std::string& token) {
                state.persistAccountSettings(lan, token);
            });
        } else if (options.lanMode) {
            pairing.setLanEnabled(true);
        }
    }

    void startDiscoveryLocked() {
        const auto tlsPort = boundTlsPort.load(std::memory_order_acquire);
        const bool secure = tlsPort > 0;

        DiscoveryService service;
        service.instanceName = "Instrument Hub";
        service.port = secure ? tlsPort : boundPort.load(std::memory_order_acquire);
        service.address = pairing.getAdvertisedAddress();
        service.txt = {
            {"path", "/remote"},
            {"version", "0.1.0"},
            {"scheme", secure ? "https" : "http"},
        };

        if (service.address.rfind("127.", 0) == 0) service.address.clear();

        if (!discovery.start(std::move(service)))
            std::cerr << "mdns: could not advertise " << Discovery::serviceType << ": "
                      << discovery.getLastError() << std::endl;
    }

    RouteContext makeRouteContext() {
        RouteContext context{state, {}, {}, {}, {}, {}, {}, {}, &pairing,
                             {},    {}, {}, {}, {}, {}, {}, {}, {}};

        {
            const std::lock_guard<std::mutex> lock(configMutex);
            context.audioProvider = audioProvider;
            context.audioApply = audioApply;
            context.audioMeasure = audioMeasure;
            context.audioMeasureDevice = audioMeasureDevice;
            context.audioWizard = audioWizard;
            context.audioWizardApply = audioWizardApply;
            context.pluginProvider = pluginProvider;
            context.pluginRescan = pluginRescan;
            context.midiProvider = midiProvider;
            context.midiUpdateInput = midiUpdateInput;
            context.projectProvider = projectProvider;
            context.projectCommand = projectCommand;
            context.projectDialogBegin = projectDialogBegin;
            context.projectDialogPoll = projectDialogPoll;
        }

        context.setLanMode = [this](bool enabled) { return applyLanMode(enabled); };

        return context;
    }

    void configureServer(httplib::Server& server) {
        server.set_pre_routing_handler(
            [this](const httplib::Request& request, httplib::Response& response) {
                if (request.method == "OPTIONS") {
                    addCorsHeaders(response);
                    response.status = 204;
                    return httplib::Server::HandlerResponse::Handled;
                }

                if (!auth.authorized(request)) {
                    setError(response, 401, "unauthorized");
                    return httplib::Server::HandlerResponse::Handled;
                }

                return httplib::Server::HandlerResponse::Unhandled;
            });
    }

    bool startTlsLocked() {
        if (boundTlsPort.load(std::memory_order_acquire) > 0) return true;

        if (options.tlsDirectory.empty()) return false;

        std::cout << "tls: starting on port " << options.tlsPort << std::endl;

        std::vector<std::string> addresses;
        juce::Array<juce::IPAddress> localAddresses;
        juce::IPAddress::findAllAddresses(localAddresses, false);

        for (const auto& address : localAddresses) {
            const auto text = address.toString().toStdString();

            if (text.rfind("127.", 0) == 0) continue;

            addresses.push_back(text);
        }

        Tls::Certificate certificate;
        std::string error;

        if (!Tls::ensureCertificate(options.tlsDirectory, addresses, certificate, error)) {
            std::cerr << "tls: " << error << std::endl;
            return false;
        }

        std::cout << "tls: certificate ready" << std::endl;

        auto server = std::make_unique<httplib::SSLServer>(certificate.certificatePath.c_str(),
                                                           certificate.privateKeyPath.c_str());

        if (!server->is_valid()) {
            std::cerr << "tls: the certificate could not be loaded" << std::endl;
            return false;
        }

        configureServer(*server);

        const auto context = makeRouteContext();
        registerRoutes(*server, context);
        webSockets.registerRoutes(*server);

        std::cout << "tls: routes registered" << std::endl;

        int port = options.tlsPort;

        if (port > 0 && !server->bind_to_port("0.0.0.0", port))
            port = server->bind_to_any_port("0.0.0.0");
        else if (port <= 0)
            port = server->bind_to_any_port("0.0.0.0");

        if (port <= 0) {
            std::cerr << "tls: could not bind port " << options.tlsPort << std::endl;
            return false;
        }

        https = std::move(server);
        boundTlsPort.store(port, std::memory_order_release);
        tlsThread = std::thread([this] { https->listen_after_bind(); });
        pairing.setEndpoint({}, port, "https");

        std::cout << "tls: listening on https://0.0.0.0:" << port << std::endl;

        return true;
    }

    void stopTlsLocked() {
        if (https != nullptr) https->stop();

        if (tlsThread.joinable()) tlsThread.join();

        https.reset();

        if (boundTlsPort.exchange(0, std::memory_order_acq_rel) > 0) {
            const auto port = boundPort.load(std::memory_order_acquire);

            if (port > 0) pairing.setEndpoint({}, port, "http");
        }
    }

    bool bindListenerLocked() {
        if (listening.load(std::memory_order_acquire)) return true;

        http = std::make_unique<httplib::Server>();

        configureServer(*http);

        const auto context = makeRouteContext();
        registerRoutes(*http, context);
        webSockets.registerRoutes(*http);

        const auto address = options.address;
        int port = nextPort;

        if (port > 0) {
            if (!http->bind_to_port(address, port)) {
                http.reset();
                return false;
            }
        } else {
            port = http->bind_to_any_port(address);

            if (port <= 0) {
                http.reset();
                return false;
            }
        }

        boundPort.store(port, std::memory_order_release);
        nextPort = port;
        listenerAddress = address;
        listening.store(true, std::memory_order_release);
        pairing.setEndpoint({}, port, "http");

        if (pairing.isLanEnabled()) {
            if (!startTlsLocked()) {
                pairing.setEndpoint({}, port, "http");
                std::cerr << "tls: continuing without HTTPS; the phone MIDI bridge is "
                             "unavailable"
                          << std::endl;
            } else {
                startDiscoveryLocked();
            }
        }

        listenerThread = std::thread([this] { http->listen_after_bind(); });

        return true;
    }

    void stopListenerLocked() {
        discovery.stop();
        stopTlsLocked();

        if (!listening.exchange(false, std::memory_order_acq_rel)) {
            if (http != nullptr) http.reset();

            return;
        }

        if (http != nullptr) http->stop();

        if (listenerThread.joinable()) listenerThread.join();

        http.reset();
        boundPort.store(0, std::memory_order_release);
    }

    void updateLanLocked() {
        if (!running.load(std::memory_order_acquire)) return;

        if (pairing.isLanEnabled()) {
            if (!startTlsLocked()) {
                pairing.setEndpoint({}, boundPort.load(std::memory_order_acquire), "http");
                discovery.stop();
                std::cerr << "tls: continuing without HTTPS; the phone MIDI bridge is "
                             "unavailable"
                          << std::endl;
                return;
            }

            startDiscoveryLocked();
            return;
        }

        discovery.stop();
        stopTlsLocked();
    }

    PairingSnapshot applyLanMode(bool enabled) {
        const auto changed = pairing.setLanEnabled(enabled);

        if (changed) {
            if (enabled)
                pairing.setEndpoint({}, options.tlsPort, "https");
            else
                pairing.setEndpoint({}, boundPort.load(std::memory_order_acquire), "http");
        }

        const auto snapshot = pairing.snapshot();

        if (changed) {
            {
                const std::lock_guard<std::mutex> lock(controlMutex);
                lanChangeRequested = true;
            }

            controlCv.notify_one();
        }

        return snapshot;
    }

    void controlLoop() {
        for (;;) {
            std::unique_lock<std::mutex> lock(controlMutex);
            controlCv.wait(lock, [this] { return controlStop || lanChangeRequested; });

            if (controlStop) return;

            lanChangeRequested = false;
            lock.unlock();

            const std::lock_guard<std::mutex> life(lifecycleMutex);
            updateLanLocked();
        }
    }

    StateStore& state;
    const Options options;

    std::atomic<bool> running{false};
    std::atomic<bool> listening{false};
    std::atomic<int> boundPort{0};

    mutable std::mutex configMutex;
    AudioStateProvider audioProvider;
    AudioApplyHandler audioApply;
    AudioMeasureHandler audioMeasure;
    AudioMeasureHandler audioMeasureDevice;
    AudioWizardHandler audioWizard;
    AudioWizardApplyHandler audioWizardApply;
    PluginInventoryProvider pluginProvider;
    PluginRescanHandler pluginRescan;
    MidiStateProvider midiProvider;
    MidiInputHandler midiUpdateInput;
    ProjectOverviewProvider projectProvider;
    ProjectCommandHandler projectCommand;
    ProjectDialogBeginHandler projectDialogBegin;
    ProjectDialogPollHandler projectDialogPoll;

    PairingManager pairing;
    Discovery discovery;

    AuthGuard auth;
    StateBroadcast broadcast;
    MeterSampler meters;
    WebSocketRoutes webSockets;

    std::unique_ptr<httplib::Server> http;
    std::unique_ptr<httplib::SSLServer> https;
    std::thread listenerThread;
    std::thread tlsThread;
    std::thread controlThread;

    mutable std::mutex lifecycleMutex;
    std::string listenerAddress;
    int nextPort = 0;
    std::atomic<int> boundTlsPort{0};

    std::mutex controlMutex;
    std::condition_variable controlCv;
    bool controlStop = false;
    bool lanChangeRequested = false;
};

Server::Server(StateStore& state) : Server(state, Options{}) {}

Server::Server(StateStore& state, Options options)
    : impl(std::make_unique<Impl>(state, std::move(options))) {}

Server::~Server() { stop(); }

bool Server::start() {
    auto& self = *impl;
    const std::lock_guard<std::mutex> life(self.lifecycleMutex);

    if (self.running.load(std::memory_order_acquire)) return true;

    if (!self.bindListenerLocked()) return false;

    self.running.store(true, std::memory_order_release);
    self.broadcast.start();
    self.meters.start();

    {
        const std::lock_guard<std::mutex> lock(self.controlMutex);
        self.controlStop = false;
        self.lanChangeRequested = false;
    }

    self.controlThread = std::thread([&self] { self.controlLoop(); });

    return true;
}

void Server::stop() {
    auto& self = *impl;

    {
        const std::lock_guard<std::mutex> lock(self.controlMutex);
        self.controlStop = true;
        self.lanChangeRequested = false;
    }

    self.controlCv.notify_all();

    self.broadcast.stop();

    if (self.controlThread.joinable()) self.controlThread.join();

    {
        const std::lock_guard<std::mutex> life(self.lifecycleMutex);
        self.running.store(false, std::memory_order_release);
        self.stopListenerLocked();
    }

    for (int spin = 0; spin < 200 && self.webSockets.getActiveConnections() > 0; ++spin) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    self.meters.stop();

    self.boundPort.store(0, std::memory_order_release);
}

bool Server::isRunning() const noexcept { return impl->running.load(std::memory_order_acquire); }

bool Server::isListening() const noexcept {
    return impl->listening.load(std::memory_order_acquire);
}

int Server::getPort() const noexcept { return impl->boundPort.load(std::memory_order_acquire); }

int Server::getTlsPort() const noexcept {
    return impl->boundTlsPort.load(std::memory_order_acquire);
}

std::string Server::getAddress() const {
    const std::lock_guard<std::mutex> life(impl->lifecycleMutex);
    return impl->listenerAddress.empty() ? impl->options.address : impl->listenerAddress;
}

void Server::setMeterProvider(MeterProvider provider) {
    impl->meters.setProvider(std::move(provider));
}

void Server::setAudioHandlers(AudioStateProvider provider, AudioApplyHandler apply,
                              AudioMeasureHandler measure, AudioMeasureHandler measureDevice) {
    const std::lock_guard<std::mutex> lock(impl->configMutex);
    impl->audioProvider = std::move(provider);
    impl->audioApply = std::move(apply);
    impl->audioMeasure = std::move(measure);
    impl->audioMeasureDevice = std::move(measureDevice);
}

void Server::setAudioWizardHandlers(AudioWizardHandler recommend, AudioWizardApplyHandler apply) {
    const std::lock_guard<std::mutex> lock(impl->configMutex);
    impl->audioWizard = std::move(recommend);
    impl->audioWizardApply = std::move(apply);
}

void Server::setPluginHandlers(PluginInventoryProvider provider, PluginRescanHandler rescan) {
    const std::lock_guard<std::mutex> lock(impl->configMutex);
    impl->pluginProvider = std::move(provider);
    impl->pluginRescan = std::move(rescan);
}

void Server::setMidiHandlers(MidiStateProvider provider, MidiInputHandler updateInput) {
    const std::lock_guard<std::mutex> lock(impl->configMutex);
    impl->midiProvider = std::move(provider);
    impl->midiUpdateInput = std::move(updateInput);
}

void Server::setNetworkMidiHandlers(NetworkMidiHandlers handlers) {
    impl->webSockets.setNetworkMidiHandlers(std::move(handlers));
}

void Server::setProjectHandlers(ProjectOverviewProvider provider, ProjectCommandHandler command) {
    const std::lock_guard<std::mutex> lock(impl->configMutex);
    impl->projectProvider = std::move(provider);
    impl->projectCommand = std::move(command);
}

void Server::setProjectDialogHandlers(ProjectDialogBeginHandler begin,
                                      ProjectDialogPollHandler poll) {
    const std::lock_guard<std::mutex> lock(impl->configMutex);
    impl->projectDialogBegin = std::move(begin);
    impl->projectDialogPoll = std::move(poll);
}

void Server::setAuthToken(std::string token) { impl->auth.setToken(std::move(token)); }

bool Server::hasAuthToken() const { return impl->auth.hasToken(); }

PairingManager& Server::getPairing() { return impl->pairing; }

const PairingManager& Server::getPairing() const { return impl->pairing; }

Discovery& Server::getDiscovery() { return impl->discovery; }

const Discovery& Server::getDiscovery() const { return impl->discovery; }

StateBroadcast& Server::getStateBroadcast() { return impl->broadcast; }

const StateBroadcast& Server::getStateBroadcast() const { return impl->broadcast; }

PairingSnapshot Server::setLanMode(bool enabled) { return impl->applyLanMode(enabled); }

bool Server::isLanModeEnabled() const { return impl->pairing.isLanEnabled(); }

std::uint64_t Server::getMeterFrameCount() const noexcept {
    return impl->webSockets.getMeterFrameCount();
}

}  // namespace Api
