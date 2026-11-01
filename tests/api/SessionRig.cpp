#include "SessionRig.h"

#include "api/SessionBridge.h"

#include <iostream>

namespace ApiTest
{

ihub::SessionConfig makeToneConfig (int slots)
{
    ihub::SessionConfig config;
    config.device.sampleRate = 48000.0;
    config.device.bufferSize = 128;
    config.device.numOutputChannels = 2;
    config.device.numInputChannels = 2;
    config.fadeMilliseconds = 3.0f;
    config.slots = ihub::Session::makeDefaultSet (ihub::SessionSlotKind::initTone);
    config.slots.resize (static_cast<std::size_t> (slots));
    return config;
}

SessionRig::SessionRig (const ihub::SessionConfig& config,
                        int readTimeoutSeconds,
                        const std::string& label,
                        const juce::File& pluginStateDirectory)
{
    if (pluginStateDirectory != juce::File())
        sessionInstance.setPluginStateDirectory (pluginStateDirectory);

    juce::String error;

    if (! sessionInstance.build (config, error))
    {
        std::cerr << label << ": FAILED to build the session: " << error << std::endl;
        return;
    }

    stateInstance = std::make_unique<Api::StateStore> (sessionInstance.getNumSlots());

    Api::Server::Options options;
    options.address = "127.0.0.1";
    options.port = 0;
    options.meterHz = 30;

    serverInstance = std::make_unique<Api::Server> (*stateInstance, options);
    Api::attachSession (*stateInstance, *serverInstance, sessionInstance);

    if (! serverInstance->start())
    {
        std::cerr << label << ": FAILED to start the server" << std::endl;
        serverInstance.reset();
        return;
    }

    clientInstance = std::make_unique<httplib::Client> ("127.0.0.1", serverInstance->getPort());
    clientInstance->set_connection_timeout (2, 0);
    clientInstance->set_read_timeout (readTimeoutSeconds, 0);
    startedFlag = true;
}

SessionRig::~SessionRig()
{
    stop();
}

void SessionRig::stop()
{
    if (serverInstance != nullptr)
        serverInstance->stop();
}

} // namespace ApiTest
