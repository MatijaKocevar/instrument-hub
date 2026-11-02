#include "Scenarios.h"

#include "TestSupport.h"

#include "Session.h"
#include "api/Server.h"
#include "api/SessionBridge.h"

#include <juce_events/juce_events.h>

#include <httplib.h>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

namespace ShutdownTest
{

double runSessionApiShutdown()
{
    auto session = std::make_unique<ihub::Session>();
    juce::String error;

    if (! session->build (makeToneConfig (8), error))
    {
        check (false, "the session builds: " + error.toStdString());
        return 0.0;
    }

    check (session->waitForSlotWarm (0, 5000), "the initial patch warms");

    auto state = std::make_unique<Api::StateStore> (session->getNumSlots());

    Api::Server::Options options;
    options.address = "127.0.0.1";
    options.port = 0;
    options.meterHz = 60;

    auto server = std::make_unique<Api::Server> (*state, options);
    Api::attachSession (*state, *server, *session);

    if (! server->start())
    {
        check (false, "the control API starts");
        return 0.0;
    }

    {
        httplib::Client client ("127.0.0.1", server->getPort());
        client.set_connection_timeout (2, 0);
        client.set_read_timeout (2, 0);

        const auto response = client.Get ("/state");
        check (response && response->status == 200,
               "the control API serves GET /state");
    }

    const auto start = Clock::now();

    server->stop();
    session->beginShutdown();
    session->closeDevice();
    server.reset();
    state.reset();
    session.reset();

    return millisecondsSince (start);
}

void simulateStoppedMessageLoop()
{
    if (auto* manager = juce::MessageManager::getInstanceWithoutCreating())
    {
        manager->stopDispatchLoop();
        manager->runDispatchLoopUntil (10);
    }
}

double runStalledLoadShutdown()
{
    const auto directory = juce::File::getSpecialLocation (juce::File::tempDirectory)
                               .getChildFile ("ihub-t185-" + juce::Uuid().toString());
    (void) directory.deleteRecursively();

    if (! directory.createDirectory().wasOk())
    {
        check (false, "the stalled-load fixture directory is created");
        return 0.0;
    }

    const auto marker = directory.getChildFile ("started");
    const auto script = directory.getChildFile ("stall-helper.sh");
    const auto bundle = directory.getChildFile ("stall.vst3");
    const auto markerPath = marker.getFullPathName().replace ("\"", "\\\"");

    const bool scriptWritten = script.replaceWithText (
        "#!/bin/sh\nprintf started > \"" + markerPath + "\"\nexec sleep 60\n",
        false,
        false,
        "\n");

    const bool bundleWritten = bundle.replaceWithText ("not a plugin bundle");

    check (scriptWritten && bundleWritten, "the stalled-load fixture is written");

    if (! scriptWritten || ! bundleWritten)
    {
        (void) directory.deleteRecursively();
        return 0.0;
    }

    (void) script.setExecutePermission (true);
    (void) ::setenv ("IHUB_SANDBOX", script.getFullPathName().toRawUTF8(), 1);

    ihub::SessionConfig config;
    config.device.sampleRate = 48000.0;
    config.device.bufferSize = 128;
    config.device.numOutputChannels = 2;
    config.device.numInputChannels = 0;
    config.initialSlot = 0;
    config.neighbourCount = 0;
    config.slots = ihub::Session::makeDefaultSet (ihub::SessionSlotKind::empty);
    config.slots.resize (4);

    auto& plugin = config.slots.front();
    plugin.kind = ihub::SessionSlotKind::plugin;
    plugin.path = bundle.getFullPathName().toStdString();
    plugin.pluginFormat = "VST3";
    plugin.pluginId = "stalled";
    plugin.pluginName = "Stalled";
    plugin.instrumentKind = "plugin";
    plugin.name = "Stalled Patch";
    plugin.patch = "stalled-patch";

    auto session = std::make_unique<ihub::Session>();
    juce::String error;

    if (! session->build (config, error))
    {
        check (false, "the stalled-load session builds: " + error.toStdString());
        (void) ::unsetenv ("IHUB_SANDBOX");
        (void) directory.deleteRecursively();
        return 0.0;
    }

    check (session->getWarmStateName (0) == std::string ("warming"),
           "the plugin load is in flight after build");

    const bool started = waitForFile (marker, 3000);
    check (started, "the stalled sandbox helper reaches its startup loop");

    const auto start = Clock::now();

    session->beginShutdown();
    session->closeDevice();
    session.reset();

    const double elapsed = millisecondsSince (start);

    (void) ::unsetenv ("IHUB_SANDBOX");
    (void) directory.deleteRecursively();

    return elapsed;
}

}
