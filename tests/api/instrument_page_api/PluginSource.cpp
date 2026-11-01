#include "PluginSource.h"

#include "../AudioMeasurements.h"
#include "../JsonSupport.h"
#include "../RequestDispatch.h"
#include "../TestSupport.h"

#include <cmath>
#include <iostream>
#include <string>

#ifndef IHUB_TESTPLUGIN_VST3
#define IHUB_TESTPLUGIN_VST3 ""
#endif

namespace ApiTest::InstrumentPageApi
{

void runPluginSource (SessionRig& rig)
{
    auto& session = rig.session();
    auto& client = rig.client();

    const juce::String testPluginPath (IHUB_TESTPLUGIN_VST3);

    if (! (testPluginPath.isNotEmpty() && juce::File (testPluginPath).exists()))
    {
        std::cout << "SKIP the VST3 source check (IHUB_TESTPLUGIN_VST3 is not set)"
                  << std::endl;
        return;
    }

    const std::string body = "{\"kind\":\"plugin\",\"name\":\"TestPlugin\","
                             "\"path\":\"" + testPluginPath.toStdString() + "\","
                             "\"pluginId\":\"com.instrumenthub.TestPlugin\","
                             "\"pluginManufacturer\":\"Instrument Hub\","
                             "\"pluginVersion\":\"1.0.0\"}";

    auto response = postWithDispatch (client, "/patches/0/source", body);
    check (response && response->status == 200, "a VST3 source is accepted");

    const bool active = session.waitForSlotWarm (0, 15000);
    check (active, "the armed slot warms the VST3 source");

    response = client.Get ("/state");

    const auto pluginPatch = patchForSlot (parseJson (response->body), 0);
    const auto pluginInstrument = property (pluginPatch, "instrument");

    check (property (pluginInstrument, "kind").toString() == "plugin",
           "/state publishes the plug-in source kind");
    check (property (pluginInstrument, "pluginName").toString() == "TestPlugin",
           "/state publishes the plug-in name");

    const auto pluginAudio = capture (session, kSettleBlocks, kMeasureBlocks);
    bool finite = ! pluginAudio.empty();

    for (const float sample : pluginAudio)
        finite = finite && std::isfinite (sample) && std::abs (sample) <= 2.0f;

    check (finite, "the hosted plug-in source processes the rendered block");
    check (session.getWarmStateName (0) == std::string ("active"),
           "the plug-in source is the active slot");

    response = postWithDispatch (client, "/patches/0/source",
                                 "{\"kind\":\"init-tone\",\"frequency\":220}");
    check (response && response->status == 200, "the slot returns to a tone source");
    check (session.waitForSlotWarm (0, 5000), "the tone source warms again");
    check (rmsOf (capture (session, kSettleBlocks, kMeasureBlocks)) > 0.01,
           "the graph renders again after a plug-in source was hosted");
}

} // namespace ApiTest::InstrumentPageApi
