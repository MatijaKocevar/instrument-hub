#include "Mixer.h"

#include "../AudioMeasurements.h"
#include "../JsonSupport.h"
#include "../RequestDispatch.h"
#include "../TestSupport.h"

#include <cmath>

namespace ApiTest::InstrumentPageApi
{

void runMixer (SessionRig& rig)
{
    auto& session = rig.session();
    auto& client = rig.client();

    auto response = client.Post ("/arm", "{\"slot\":0}", "application/json");
    check (response && response->status == 200, "POST /arm 0 returns 200");
    check (session.waitForSlotActive (0, 5000),
           "slot 0 warms and becomes the active patch");

    response = client.Get ("/state");

    const auto stateDocument = response ? parseJson (response->body) : juce::var {};
    const auto patch = patchForSlot (stateDocument, 0);

    check (patch.isObject(), "GET /state carries the armed patch");
    check (property (patch, "name").isString(), "the patch has a name");
    check (property (patch, "warmState").toString() == "active",
           "the armed patch reports the active warm state");
    check (property (patch, "latencySamples").isInt(),
           "the patch publishes its chain latency");
    check (property (patch, "instrument").isObject(),
           "the patch publishes its instrument source");

    const double baselineRms = rmsOf (capture (session, kSettleBlocks, kMeasureBlocks));

    check (baselineRms > 0.01, "the armed patch renders a non-silent baseline");

    response = patchWithDispatch (client, "/patches/0", "{\"volumeDb\":-20}");
    check (response && response->status == 200, "PATCH volumeDb -20 returns 200");

    const double quietRms = rmsOf (capture (session, kSettleBlocks, kMeasureBlocks));
    const double expectedRatio = std::pow (10.0, -20.0 / 20.0);

    check (std::abs (quietRms / baselineRms - expectedRatio) < 0.02,
           "-20 dB on the fader drops the rendered level to a tenth (ratio "
               + std::to_string (quietRms / baselineRms) + ")");

    response = client.Get ("/state");
    check (std::abs (numberProperty (patchForSlot (parseJson (response->body), 0),
                                     "volumeDb")
                     + 20.0) < 1.0e-9,
           "the fader value is published in /state");

    response = patchWithDispatch (client, "/patches/0", "{\"volumeDb\":0}");
    check (response && response->status == 200, "PATCH volumeDb 0 returns 200");

    const double restoredRms = rmsOf (capture (session, kSettleBlocks, kMeasureBlocks));

    check (std::abs (restoredRms / baselineRms - 1.0) < 0.02,
           "returning the fader to 0 dB restores the rendered level");

    response = patchWithDispatch (client, "/patches/0", "{\"mute\":true}");
    check (response && response->status == 200, "PATCH mute true returns 200");

    const double mutedRms = rmsOf (capture (session, kSettleBlocks, kMeasureBlocks));

    check (mutedRms < 1.0e-5, "mute silences the rendered slot");

    response = patchWithDispatch (client, "/patches/0", "{\"mute\":false}");
    check (response && response->status == 200, "PATCH mute false returns 200");
    check (rmsOf (capture (session, kSettleBlocks, kMeasureBlocks)) > 0.01,
           "unmuting restores the rendered slot");

    response = patchWithDispatch (client, "/patches/0", "{\"pan\":-0.5}");
    check (response && response->status == 200, "PATCH pan -0.5 returns 200");
    check (std::abs (numberProperty (patchForSlot (parseJson (response->body), 0),
                                     "pan")
                     + 0.5) < 1.0e-9,
           "the balance is published in /state");

    response = patchWithDispatch (client, "/patches/0", "{\"volumeDb\":99}");
    check (response && response->status == 400, "an out-of-range fader value is rejected");
}

}
