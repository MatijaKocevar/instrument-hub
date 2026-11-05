#include "track_grid/Rig.h"

#include "track_grid/Constants.h"
#include "track_grid/RigConfig.h"

#include "api/SessionBridge.h"
#include "app/surface/TrackSurface.h"
#include "controller/LaunchpadProtocol.h"
#include "plugins/PluginInstrument.h"
#include "plugins/SandboxHost.h"

#include <cmath>
#include <utility>
#include <vector>

#ifndef IHUB_TRACKS_YAML
 #define IHUB_TRACKS_YAML "shared/tracks-set.yaml"
#endif

#ifndef IHUB_TESTPLUGIN_VST3
 #define IHUB_TESTPLUGIN_VST3 ""
#endif

namespace TrackGridE2E
{

namespace
{

bool fail (std::string& errorText, const juce::String& message)
{
    errorText = message.toStdString();
    return false;
}

float normalisedFrequency (double hertz) noexcept
{
    return static_cast<float> ((hertz - 20.0) / (20000.0 - 20.0));
}

}

Rig::Rig() = default;

Rig::~Rig()
{
    if (serverInstance != nullptr)
        serverInstance->stop();

    if (directory != juce::File())
        directory.deleteRecursively();
}

bool Rig::build (std::string& errorText)
{
    directory = juce::File::getCurrentWorkingDirectory()
                    .getChildFile ("e2e-track-grid-" + juce::Uuid().toString());

    if (! directory.createDirectory().wasOk())
        return fail (errorText, "the track project directory could not be created");

    projectFile = directory.getChildFile ("Tracks.ihub");

    if (! makeTrackProject (juce::File (IHUB_TRACKS_YAML), projectFile, errorText))
        return false;

    ihub::SessionConfig config;

    if (! loadTrackConfig (projectFile, juce::String (IHUB_TESTPLUGIN_VST3),
                           config, errorText))
        return false;

    baselineMemory = readProcessMemory();

    juce::String error;

    if (! sessionInstance.build (config, error))
        return fail (errorText, "the eight-track session builds: " + error);

    for (int slot = 0; slot < kTrackCount; ++slot)
        if (! sessionInstance.waitForSlotWarm (slot, 30000))
            return fail (errorText, "track slot " + juce::String (slot)
                                        + " did not warm its TestPlugin");

    if (! sessionInstance.waitForWarmIdle (30000))
        return fail (errorText, "the warm pool did not settle");

    warmMemory = readProcessMemory();

    for (int slot = 0; slot < kTrackCount; ++slot)
    {
        const int processId = sessionInstance.getSandboxProcessId (slot);

        if (processId <= 0)
            continue;

        const auto childMemory = readChildProcessMemory (processId);
        warmMemory.currentKb += childMemory.currentKb;
        warmMemory.peakKb += childMemory.peakKb;
    }

    for (int slot = 0; slot < kTrackCount; ++slot)
    {
        const auto frequency = normalisedFrequency (
            kFrequencies[static_cast<std::size_t> (slot)]);

        if (auto* sandbox = sessionInstance.getSandboxSlotControl (slot))
        {
            if (! sandbox->isRunning())
                return fail (errorText, "track slot " + juce::String (slot)
                                            + " sandbox is not running");

            const std::vector<ihub::plugins::SandboxParameterEdit> edits {
                { "Gain", 0.0f },
                { "Sine Level", 1.0f },
                { "Frequency", frequency }
            };

            if (! sandbox->setParameters (edits))
                return fail (errorText, "track slot " + juce::String (slot)
                                            + " rejected its TestPlugin parameters");

            continue;
        }

        auto* instrument = sessionInstance.getPluginInstrument (slot);

        if (instrument == nullptr)
            return fail (errorText, "track slot " + juce::String (slot)
                                        + " has no live TestPlugin");

        (void) instrument->setParameterValue ("Gain", 0.0f);
        (void) instrument->setParameterValue ("Sine Level", 1.0f);
        (void) instrument->setParameterValue ("Frequency", frequency);
    }

    stateInstance = std::make_unique<Api::StateStore> (sessionInstance.getNumSlots());

    Api::Server::Options options;
    options.address = "127.0.0.1";
    options.port = 0;

    serverInstance = std::make_unique<Api::Server> (*stateInstance, options);
    Api::attachSession (*stateInstance, *serverInstance, sessionInstance);

    if (! serverInstance->start())
        return fail (errorText, "the track control API did not start");

    clientInstance = std::make_unique<httplib::Client> (
        "127.0.0.1", serverInstance->getPort());
    clientInstance->set_connection_timeout (2, 0);
    clientInstance->set_read_timeout (30, 0);

    Controller::LaunchpadDriver::DriverListeners driverListeners;

    driverListeners.padEvent = [this] (int pad, bool pressed)
    {
        mapping.handlePad (pad, pressed);
    };

    driver.setListeners (std::move (driverListeners));

    Controller::Mapping::Listeners listeners;

    listeners.armSlot = [this] (int slot)
    {
        (void) stateInstance->setArmedSlot (slot);
    };

    listeners.padEvent = [this] (int,
                                 const Controller::PadBinding* binding,
                                 bool pressed)
    {
        if (pressed && binding != nullptr)
            ihub::app::applyTrackPadAction (*stateInstance, *binding);
    };

    mapping.setListeners (std::move (listeners));

    Controller::MappingError mappingError;

    if (! mapping.loadProject (projectFile.getFullPathName().toStdString(),
                               &mappingError))
        return fail (errorText, "the track mapping loads: "
                                    + juce::String (mappingError.message));

    attachLedStream (driver, leds);

    if (! stateInstance->setArmedSlot (0))
        return fail (errorText, "the first track could not be armed");

    if (! sessionInstance.waitForSlotActive (0, 5000))
        return fail (errorText, "the first track did not become active");

    for (int attempt = 0; attempt < 200; ++attempt)
    {
        renderBlocks (2);

        bool produced = true;

        for (int slot = 0; slot < kTrackCount; ++slot)
            if (auto* sandbox = sessionInstance.getSandboxSlotControl (slot);
                sandbox != nullptr && sandbox->getPlayedBlockCount() == 0)
                produced = false;

        if (produced)
            break;

        juce::Thread::sleep (2);
    }

    return true;
}

}
