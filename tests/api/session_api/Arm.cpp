#include "Arm.h"

#include "../AudioMeasurements.h"
#include "../JsonSupport.h"
#include "../TestSupport.h"

#include <cmath>
#include <iostream>

namespace ApiTest::SessionApi
{

void runArm (SessionRig& rig)
{
    auto& session = rig.session();
    auto& client = rig.client();

    auto response = client.Post ("/arm", "{\"slot\":0}", "application/json");
    check (response && response->status == 200, "POST /arm 0 returns 200");
    check (session.waitForSlotActive (0, 5000),
           "slot 0 warms and becomes the active patch");

    renderBlocks (session, 16);

    const double frequency0 = captureFrequency (session, 1.0);

    std::cout << "session-api-test: slot 0 frequency=" << frequency0
              << " Hz gain=" << session.getSwitcher().getSlotGain (0)
              << " peak=" << session.getSlotMeterPeak (0)
              << " master=" << session.getMasterMeterPeak() << std::endl;

    check (std::abs (frequency0 - kSlot0Frequency) < 2.0,
           "arming slot 0 makes its 110 Hz tone audible");
    check (session.getSwitcher().getSlotGain (0) > 0.99f,
           "SlotSwitcher faded slot 0 to full gain");
    check (session.getSwitcher().getSlotGain (1) < 0.01f,
           "SlotSwitcher is not playing slot 1");
    check (session.getSlotMeterPeak (0) > 0.01f,
           "slot 0 meter reads a non-zero peak from the real output");
    check (session.getMasterMeterPeak() > 0.01f,
           "master meter reads a non-zero peak from the real bus");

    auto stateResponse = client.Get ("/state");

    if (stateResponse && stateResponse->status == 200)
    {
        const auto body = parseJson (stateResponse->body);
        check (intProperty (body, "armedSlot") == 0,
               "GET /state reports slot 0 armed");

        const auto slotsVar = property (body, "slots");
        const auto* slots = slotsVar.getArray();
        const bool slot0Active = slots != nullptr && slots->size() > 0
                                 && property ((*slots)[0], "state").toString() == "active"
                                 && numberProperty ((*slots)[0], "gain") > 0.99;
        check (slot0Active, "GET /state reports slot 0's real state \"active\" and gain 1");
    }
    else
    {
        check (false, "GET /state returns 200 while slot 0 is armed");
    }

    response = client.Post ("/arm", "{\"slot\":1}", "application/json");
    check (response && response->status == 200, "POST /arm 1 returns 200");
    check (session.waitForSlotActive (1, 5000),
           "slot 1 warms and becomes the active patch");

    renderBlocks (session, 16);

    const double frequency1 = captureFrequency (session, 1.0);

    std::cout << "session-api-test: slot 1 frequency=" << frequency1
              << " Hz gain=" << session.getSwitcher().getSlotGain (1)
              << " peak=" << session.getSlotMeterPeak (1)
              << " slot0_gain=" << session.getSwitcher().getSlotGain (0)
              << " master=" << session.getMasterMeterPeak() << std::endl;

    check (std::abs (frequency1 - kSlot1Frequency) < 2.0,
           "arming slot 1 changes the audible tone to 123.47 Hz");
    check (session.getSwitcher().getSlotGain (1) > 0.99f,
           "SlotSwitcher faded slot 1 to full gain");
    check (session.getSwitcher().getSlotGain (0) < 0.01f,
           "SlotSwitcher faded slot 0 out");
    check (session.getSlotMeterPeak (1) > 0.01f,
           "slot 1 meter reads a non-zero peak from the real output");

    stateResponse = client.Get ("/state");

    if (stateResponse && stateResponse->status == 200)
    {
        const auto body = parseJson (stateResponse->body);
        check (intProperty (body, "armedSlot") == 1,
               "GET /state reports slot 1 armed");

        const auto slotsVar = property (body, "slots");
        const auto* slots = slotsVar.getArray();

        const bool slot1Active = slots != nullptr && slots->size() > 1
                                 && property ((*slots)[1], "state").toString() == "active"
                                 && numberProperty ((*slots)[1], "gain") > 0.99;
        const bool slot0Idle = slots != nullptr && slots->size() > 0
                               && property ((*slots)[0], "state").toString() == "idle"
                               && numberProperty ((*slots)[0], "gain") < 0.01;

        check (slot1Active, "GET /state reports slot 1's real state \"active\"");
        check (slot0Idle, "GET /state reports slot 0's real state \"idle\"");
    }
    else
    {
        check (false, "GET /state returns 200 while slot 1 is armed");
    }
}

} // namespace ApiTest::SessionApi
