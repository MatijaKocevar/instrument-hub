#include "ControlApi.h"

#include "CommandLine.h"

#include "Session.h"
#include "api/MidiBridge.h"
#include "api/PluginBridge.h"
#include "api/Server.h"
#include "api/SessionBridge.h"
#include "audio/AudioSettings.h"
#include "plugins/PluginInventory.h"

#include <iostream>
#include <utility>

namespace ihub::app
{

ControlApi::ControlApi() = default;

ControlApi::~ControlApi()
{
    stopServer();
    detachMidi();
    resetServer();
    resetAudio();
}

bool ControlApi::start (const juce::String& commandLine,
                        ihub::Session* session,
                        bool required,
                        std::function<void()> midiRestore)
{
    Api::Server::Options options;

    if (const auto port = getCommandLineValue (commandLine, "--api-port"); port.isNotEmpty())
        options.port = port.getIntValue();

    if (const auto address = getCommandLineValue (commandLine, "--api-host"); address.isNotEmpty())
        options.address = address.toStdString();

    options.authToken = getCommandLineValue (commandLine, "--api-token").toStdString();
    options.lanMode = commandLine.contains ("--lan") || commandLine.contains ("--lan-mode");

    const int apiSlots = session != nullptr ? session->getNumSlots()
                                            : ihub::Session::defaultLogicalSlots;
    apiState = std::make_unique<Api::StateStore> (apiSlots);
    apiServer = std::make_unique<Api::Server> (*apiState, options);
    pluginInventory = std::make_unique<ihub::plugins::PluginInventory>();
    Api::attachPluginInventory (*apiServer, *pluginInventory);

    const auto inventory = pluginInventory->snapshot();

    if (inventory.scanned)
    {
        std::cout << "plugins: " << inventory.total
                  << " installed VST3 type(s) in the inventory" << std::endl;
    }
    else if (pluginInventory->requestRescan())
    {
        std::cout << "plugins: scanning VST3 directories in the background"
                  << std::endl;
    }

    midiBridge = std::make_unique<Api::MidiBridge>();
    midiBridge->setRestoreHandler (std::move (midiRestore));

    if (session != nullptr)
        midiBridge->setMidiSink ([session] (const juce::MidiMessage& message)
        {
            return session != nullptr && session->pushMidi (message);
        });

    apiServer->setMidiHandlers (
        [this] { return midiBridge->snapshot(); },
        [this] (const std::string& id, bool enabled, std::string& error)
        {
            return midiBridge->setInputEnabled (id, enabled, error);
        },
        [this] (std::string& error) { return midiBridge->runPadTest (error); });

    audioSettings = std::make_unique<AudioEngine::AudioSettingsBridge>();

    if (session != nullptr)
    {
        audioSettings->setDeviceManager (&session->getDevices());
        Api::attachSession (*apiState, *apiServer, *session);
        audioSettings->setTimingMonitor (&session->getEngine().getTimingMonitor());
    }

    if (session == nullptr
        && (commandLine.contains ("--audio")
            || commandLine.contains ("--audio-loopback")))
    {
        juce::String audioError;

        if (audioSettings->openLoopback (&audioError))
        {
            std::cout << "audio: opened Loopback at "
                      << audioSettings->snapshot().sampleRate << " Hz / "
                      << audioSettings->snapshot().bufferSize << " frames"
                      << std::endl;
        }
        else
        {
            std::cerr << "audio: could not open the Loopback device: "
                      << audioError << std::endl;
        }
    }

    apiServer->setAudioHandlers (
        [this] { return audioSettings->snapshot(); },
        [this] (const Api::AudioSettingsPatch& patch, std::string& error)
        {
            return audioSettings->apply (patch, error);
        },
        [this] { return audioSettings->measure(); });

    if (! apiServer->start())
    {
        std::cerr << "api: FAILED to bind " << options.address << ":" << options.port
                  << (apiServer->hasAuthToken() ? " (auth enabled)" : "")
                  << std::endl;
        apiServer.reset();
        midiBridge.reset();
        audioSettings.reset();
        apiState.reset();

        if (! required)
            std::cerr << "api: continuing without the control server" << std::endl;

        return false;
    }

    std::cout << "api: listening on http://" << apiServer->getAddress() << ":"
              << apiServer->getPort()
              << " meters=" << options.meterHz << "Hz"
              << " auth=" << (apiServer->hasAuthToken() ? "enabled" : "disabled")
              << " lan=" << (apiServer->isLanModeEnabled() ? "enabled" : "disabled")
              << std::endl;

    return true;
}

void ControlApi::stopServer()
{
    if (apiServer != nullptr)
        apiServer->stop();
}

void ControlApi::detachMidi()
{
    if (midiBridge != nullptr)
    {
        midiBridge->setLaunchpad (nullptr);
        midiBridge.reset();
    }
}

void ControlApi::resetServer()
{
    apiServer.reset();
}

void ControlApi::resetAudio()
{
    audioSettings.reset();
    apiState.reset();
}

}
