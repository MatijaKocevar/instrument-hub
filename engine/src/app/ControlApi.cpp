#include "ControlApi.h"

#include "CommandLine.h"
#include "MidiDeviceWatcher.h"
#include "project/ProjectDialog.h"
#include "project/ProjectLifecycle.h"

#include "Session.h"
#include "api/MidiBridge.h"
#include "api/PluginBridge.h"
#include "api/Server.h"
#include "api/SessionBridge.h"
#include "audio/AudioSettings.h"
#include "plugins/PluginInventory.h"
#include "settings/SettingsStore.h"

#include <cstdint>
#include <iostream>
#include <utility>

namespace ihub::app {

ControlApi::ControlApi() = default;

ControlApi::~ControlApi() {
    stopServer();
    detachMidi();
    resetServer();
    resetAudio();
}

bool ControlApi::start(const juce::String& commandLine, ihub::Session* session, bool required,
                       const juce::File& projectFile, std::function<void()> midiRestore,
                       ihub::settings::SettingsStore* settings,
                       const Api::AudioRestoreState& audioRestore, bool persistProjectPath,
                       std::function<void(const juce::File&)> projectOpened) {
    Api::Server::Options options;

    if (const auto port = getCommandLineValue(commandLine, "--api-port"); port.isNotEmpty())
        options.port = port.getIntValue();

    if (const auto address = getCommandLineValue(commandLine, "--api-host"); address.isNotEmpty())
        options.address = address.toStdString();

    options.authToken = getCommandLineValue(commandLine, "--api-token").toStdString();
    options.lanMode = commandLine.contains("--lan") || commandLine.contains("--lan-mode");

    const auto tlsDirectory =
        settings != nullptr
            ? settings->getFile().getParentDirectory().getChildFile("tls")
            : ihub::settings::SettingsStore::defaultFile().getParentDirectory().getChildFile("tls");

    options.tlsDirectory = tlsDirectory.getFullPathName().toStdString();

    const int apiSlots =
        session != nullptr ? session->getNumSlots() : ihub::Session::defaultLogicalSlots;
    apiState = std::make_unique<Api::StateStore>(apiSlots, settings);
    apiServer = std::make_unique<Api::Server>(*apiState, options);
    projectDialog = std::make_unique<ProjectDialog>();
    apiServer->setProjectDialogHandlers(
        [this](const Api::ProjectDialogRequest& request, std::uint64_t& id, std::string& error) {
            return projectDialog != nullptr && projectDialog->begin(request, id, error);
        },
        [this](std::uint64_t id, Api::ProjectDialogStatus& status, std::string& error) {
            return projectDialog != nullptr && projectDialog->poll(id, status, error);
        });
    pluginInventory = std::make_unique<ihub::plugins::PluginInventory>();
    Api::attachPluginInventory(*apiServer, *pluginInventory);

    const auto inventory = pluginInventory->snapshot();

    if (inventory.scanned) {
        std::cout << "plugins: " << inventory.total << " installed VST3 type(s) in the inventory"
                  << std::endl;
    } else if (pluginInventory->requestRescan()) {
        std::cout << "plugins: scanning VST3 directories in the background" << std::endl;
    }

    midiBridge = std::make_unique<Api::MidiBridge>();
    midiBridge->setSettingsStore(settings);
    midiBridge->setRestoreHandler(std::move(midiRestore));

    if (session != nullptr)
        midiBridge->setMidiSink([session](const juce::MidiMessage& message) {
            return session != nullptr && session->pushMidi(message);
        });

    apiServer->setMidiHandlers([this] { return midiBridge->snapshot(); },
                               [this](const Api::MidiInputPatch& patch, std::string& error) {
                                   return midiBridge->updateInput(patch, error);
                               });

    apiServer->setNetworkMidiHandlers(Api::NetworkMidiHandlers{
        [this](const std::string& id, const std::string& name, std::uint64_t connectionSession) {
            if (midiBridge != nullptr) midiBridge->attachNetworkClient(id, name, connectionSession);

            if (apiState != nullptr) apiState->notifyChanged("midi", -1);
        },
        [this](const std::string& id, std::uint64_t connectionSession) {
            if (midiBridge != nullptr) midiBridge->detachNetworkClient(id, connectionSession);

            if (apiState != nullptr) apiState->notifyChanged("midi", -1);
        },
        [this](const std::string& id, std::uint64_t connectionSession, const std::uint8_t* data,
               int size) {
            return midiBridge != nullptr &&
                   midiBridge->networkMidi(id, connectionSession, data, size);
        },
    });

    midiDeviceWatcher = std::make_unique<MidiDeviceWatcher>(*apiState);

    audioSettings = std::make_unique<AudioEngine::AudioSettingsBridge>();
    audioSettings->setSettingsStore(settings);
    audioSettings->setRestoreState(audioRestore);

    if (session != nullptr) {
        audioSettings->setDeviceManager(&session->getDevices());
        Api::attachSession(*apiState, *apiServer, *session);
        audioSettings->setTimingMonitor(&session->getEngine().getTimingMonitor());

        projectLifecycle =
            std::make_unique<ProjectLifecycle>(*session, *apiState, *apiServer, settings);
        projectLifecycle->setOpenedHandler(std::move(projectOpened));

        if (projectFile != juce::File()) {
            juce::String projectError;

            if (projectLifecycle->attach(projectFile, persistProjectPath, projectError))
                std::cout << "project: autosave targets " << projectFile.getFullPathName()
                          << std::endl;
            else
                std::cerr << "project: autosave is off: " << projectError << std::endl;
        }
    }

    if (session == nullptr &&
        (commandLine.contains("--audio") || commandLine.contains("--audio-loopback"))) {
        juce::String audioError;

        if (audioSettings->openLoopback(&audioError)) {
            std::cout << "audio: opened Loopback at " << audioSettings->snapshot().sampleRate
                      << " Hz / " << audioSettings->snapshot().bufferSize << " frames" << std::endl;
        } else {
            std::cerr << "audio: could not open the Loopback device: " << audioError << std::endl;
        }
    }

    const auto measureDevice = [session] {
        if (session != nullptr && session->getDevices().isOpen()) return session->measureLatency();

        Api::AudioMeasurementState measurement;
        measurement.error = "the audio device is not open";
        return measurement;
    };

    audioSettings->setDeviceMeasureHandler(measureDevice);

    apiServer->setAudioHandlers([this] { return audioSettings->snapshot(); },
                                [this](const Api::AudioSettingsPatch& patch, std::string& error) {
                                    return audioSettings->apply(patch, error);
                                },
                                [this] { return audioSettings->measure(); }, measureDevice);

    apiServer->setAudioWizardHandlers(
        [this] { return audioSettings->recommendSetup(); },
        [this](std::string& error) { return audioSettings->applyRecommendation(error); });

    if (!apiServer->start()) {
        std::cerr << "api: FAILED to bind " << options.address << ":" << options.port
                  << (apiServer->hasAuthToken() ? " (auth enabled)" : "") << std::endl;
        resetProject();
        apiServer.reset();
        midiBridge.reset();
        audioSettings.reset();
        apiState.reset();

        if (!required) std::cerr << "api: continuing without the control server" << std::endl;

        return false;
    }

    std::cout << "api: listening on http://" << apiServer->getAddress() << ":"
              << apiServer->getPort() << " meters=" << options.meterHz << "Hz"
              << " auth=" << (apiServer->hasAuthToken() ? "enabled" : "disabled")
              << " lan=" << (apiServer->isLanModeEnabled() ? "enabled" : "disabled") << std::endl;

    return true;
}

void ControlApi::stopServer() {
    if (apiServer != nullptr) apiServer->stop();
}

void ControlApi::detachMidi() {
    midiDeviceWatcher.reset();

    if (midiBridge != nullptr) {
        midiBridge->setLaunchpad(nullptr);
        midiBridge.reset();
    }
}

void ControlApi::resetServer() {
    resetProject();
    apiServer.reset();
    projectDialog.reset();
}

void ControlApi::resetAudio() {
    resetProject();
    audioSettings.reset();
    apiState.reset();
}

void ControlApi::resetProject() {
    if (projectLifecycle != nullptr) {
        projectLifecycle->shutdown();
        projectLifecycle.reset();
    }

    if (apiState != nullptr) {
        apiState->setEditHandler({});
        apiState->setProjectStateProvider({});
    }
}

void ControlApi::flushProject() {
    if (projectLifecycle == nullptr) return;

    projectLifecycle->flush();
}

}  // namespace ihub::app
