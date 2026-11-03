#pragma once

#include "Session.h"
#include "api/Server.h"

#include <httplib.h>
#include <juce_core/juce_core.h>

#include <memory>
#include <string>

namespace ApiTest
{

ihub::SessionConfig makeToneConfig (int slots);
ihub::SessionConfig makeTrackConfig (int slots);

class SessionRig
{
public:
    SessionRig (const ihub::SessionConfig& config,
                int readTimeoutSeconds,
                const std::string& label,
                const juce::File& pluginStateDirectory = {});

    ~SessionRig();

    SessionRig (const SessionRig&) = delete;
    SessionRig& operator= (const SessionRig&) = delete;

    bool started() const noexcept { return startedFlag; }

    ihub::Session& session() noexcept { return sessionInstance; }

    Api::StateStore& state() noexcept { return *stateInstance; }

    Api::Server& server() noexcept { return *serverInstance; }

    httplib::Client& client() noexcept { return *clientInstance; }

    int port() const noexcept { return serverInstance != nullptr ? serverInstance->getPort() : 0; }

    void stop();

private:
    ihub::Session sessionInstance;
    std::unique_ptr<Api::StateStore> stateInstance;
    std::unique_ptr<Api::Server> serverInstance;
    std::unique_ptr<httplib::Client> clientInstance;
    bool startedFlag = false;
};

}
