#include "Check.h"
#include "pad_audio/Constants.h"
#include "pad_audio/GridLeds.h"
#include "pad_audio/PagesScenario.h"
#include "pad_audio/Render.h"
#include "pad_audio/Rig.h"

#include "Session.h"
#include "api/Server.h"
#include "api/SessionBridge.h"

#include <httplib.h>

#include <iostream>
#include <string>

#ifndef IHUB_SET_YAML
 #define IHUB_SET_YAML "shared/set.yaml"
#endif

namespace PadAudio
{
using E2E::check;


PagesResult runPagesScenario()
{
    PagesResult result;

    auto config = makeConfig (3.0f);
    config.slots[static_cast<std::size_t> (kPagePad)].colorIndex = 1;
    config.slots[static_cast<std::size_t> (kPageIdlePad)].colorIndex = 2;

    ihub::Session session;
    juce::String buildError;

    if (! session.build (config, buildError))
    {
        result.error = "session: " + buildError.toStdString();
        return result;
    }

    PadRig rig (session);
    std::string mappingError;

    if (! rig.load (IHUB_SET_YAML, mappingError))
    {
        result.error = "set.yaml: " + mappingError;
        return result;
    }

    session.armSlot (0);

    if (! session.waitForSlotActive (0, 5000))
    {
        result.error = "slot 0 did not become active";
        return result;
    }

    Api::StateStore state (session.getNumSlots());
    Api::Server::Options options;
    options.address = "127.0.0.1";
    options.port = 0;
    Api::Server server (state, options);
    Api::attachSession (state, server, session);

    if (! server.start())
    {
        result.error = "the control API did not start";
        return result;
    }

    httplib::Client client ("127.0.0.1", server.getPort());

    const auto click = client.Post ("/arm",
                                    R"({"slot":10})",
                                    "application/json");

    result.clickSlot = kPagePad;

    if (! click)
    {
        result.error = "POST /arm did not answer";
        return result;
    }

    auto clickBody = juce::JSON::parse (juce::String (click->body));
    const auto* clickObject = clickBody.getDynamicObject();
    const int echoedSlot = clickObject != nullptr
                               ? static_cast<int> (clickObject->getProperty (
                                     juce::Identifier ("armedSlot")))
                               : -1;

    result.clickArmed = session.getArmedSlot();

    check (click->status == 200, "the grid click's POST /arm answers 200");
    check (echoedSlot == kPagePad,
           "POST /arm echoes the clicked slot " + std::to_string (kPagePad));
    check (result.clickArmed == kPagePad,
           "the grid click arms slot " + std::to_string (kPagePad));
    check (state.getArmedSlot() == kPagePad,
           "the API state mirrors the grid click's arm");

    if (! session.waitForSlotActive (kPagePad, 5000))
    {
        result.error = "the clicked slot did not become active";
        return result;
    }

    result.clickRmsDbfs = renderRms (session, kPageSettleBlocks, kPageRmsBlocks);
    check (result.clickRmsDbfs > kMinimumRmsDbfs,
           "the clicked patch renders non-silent audio");

    rig.press (0);
    check (session.waitForSlotActive (0, 5000),
           "pad 0 arms slot 0 before the page pad press");
    (void) renderRms (session, 2, 2);

    rig.press (kPagePad);
    result.pad = kPagePad;
    result.padArmed = session.getArmedSlot();

    check (result.padArmed == kPagePad,
           "the Launchpad pad arms the same slot " + std::to_string (kPagePad));

    if (! session.waitForSlotActive (kPagePad, 5000))
    {
        result.error = "the pad-armed slot did not become active";
        return result;
    }

    result.padRmsDbfs = renderRms (session, kPageSettleBlocks, kPageRmsBlocks);
    check (result.padRmsDbfs > kMinimumRmsDbfs,
           "the pad-armed patch renders non-silent audio");

    result.samePatch = result.clickArmed == result.padArmed;
    check (result.samePatch, "the grid click and the Launchpad pad armed one patch");

    const auto snapshot = state.snapshot();
    const Api::PatchState* armedPatch = nullptr;
    const Api::PatchState* idlePatch = nullptr;

    for (const auto& family : snapshot.families)
    {
        for (const auto& patch : family.patches)
        {
            if (patch.slot == kPagePad)
                armedPatch = &patch;

            if (patch.slot == kPageIdlePad)
                idlePatch = &patch;
        }
    }

    Controller::LaunchpadDriver ledDriver;
    const auto leds = captureGridLeds (snapshot, ledDriver);

    if (const auto found = leds.find (kPagePad); found != leds.end())
        result.armedLed = found->second;

    if (const auto found = leds.find (kPageIdlePad); found != leds.end())
        result.idleLed = found->second;

    result.patchColorIndex = idlePatch != nullptr ? idlePatch->colorIndex : -1;
    result.idleExpected = Launchpad::velocityForPatchColour (result.patchColorIndex);

    if (armedPatch != nullptr)
        check (armedPatch->colorIndex != -1, "the armed patch publishes a colour index");

    check (result.armedLed.seen, "the armed pad received an LED message");
    check (result.armedLed.velocity
               == Launchpad::velocityFor (Launchpad::Colour::Red),
           "the armed pad LED is red");
    check (result.armedLed.mode == static_cast<int> (Launchpad::LedMode::Static),
           "the armed pad LED is static");

    check (result.idleLed.seen, "an idle pad received an LED message");
    check (result.idleLed.velocity == result.idleExpected,
           "the idle pad LED matches its patch colour (velocity "
               + std::to_string (static_cast<int> (result.idleExpected)) + ")");
    check (result.idleLed.velocity
               != Launchpad::velocityFor (Launchpad::Colour::Red),
           "the idle pad is not painted as the armed pad");

    std::cout << "  pages: click_slot=" << result.clickSlot
              << " click_armed=" << result.clickArmed
              << " pad=" << result.pad
              << " pad_armed=" << result.padArmed
              << " same_patch=" << (result.samePatch ? "true" : "false")
              << " click_rms_dbfs=" << result.clickRmsDbfs
              << " pad_rms_dbfs=" << result.padRmsDbfs
              << " armed_led_velocity="
              << static_cast<int> (result.armedLed.velocity)
              << " armed_led_mode=" << result.armedLed.mode
              << " idle_pad=" << kPageIdlePad
              << " idle_colour_index=" << result.patchColorIndex
              << " idle_led_velocity="
              << static_cast<int> (result.idleLed.velocity)
              << " idle_led_expected=" << static_cast<int> (result.idleExpected)
              << std::endl;

    result.ran = true;
    return result;
}

}
