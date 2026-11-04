#pragma once

#include "Discovery.h"
#include "Pairing.h"
#include "StateStore.h"

#include <cstdint>
#include <memory>
#include <string>

namespace Api
{

class StateBroadcast;

class Server
{
public:
    struct Options
    {
        std::string address = "127.0.0.1";
        int port = 8080;

        std::string authToken;

        bool lanMode = false;

        int meterHz = 30;
    };

    explicit Server (StateStore& state);
    Server (StateStore& state, Options options);
    ~Server();

    Server (const Server&) = delete;
    Server& operator= (const Server&) = delete;

    bool start();
    void stop();

    bool isRunning() const noexcept;
    bool isListening() const noexcept;

    int getPort() const noexcept;
    std::string getAddress() const;

    void setMeterProvider (MeterProvider provider);

    void setAudioHandlers (AudioStateProvider provider,
                           AudioApplyHandler apply,
                           AudioMeasureHandler measure,
                           AudioMeasureHandler measureDevice = {});

    void setAudioWizardHandlers (AudioWizardHandler recommend,
                                 AudioWizardApplyHandler apply);

    void setPluginHandlers (PluginInventoryProvider provider,
                            PluginRescanHandler rescan);

    void setMidiHandlers (MidiStateProvider provider,
                          MidiInputHandler enableInput,
                          MidiPadTestHandler padTest);

    void setProjectHandlers (ProjectOverviewProvider provider,
                             ProjectCommandHandler command);

    void setAuthToken (std::string token);
    bool hasAuthToken() const;

    PairingManager& getPairing();
    const PairingManager& getPairing() const;

    Discovery& getDiscovery();
    const Discovery& getDiscovery() const;

    StateBroadcast& getStateBroadcast();
    const StateBroadcast& getStateBroadcast() const;

    PairingSnapshot setLanMode (bool enabled);
    bool isLanModeEnabled() const;

    std::uint64_t getMeterFrameCount() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

}
