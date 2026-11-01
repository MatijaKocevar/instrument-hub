#include "Source.h"

#include "../AudioMeasurements.h"
#include "../FxChainJson.h"
#include "../JsonSupport.h"
#include "../RequestDispatch.h"
#include "../TestSupport.h"

#include <cmath>
#include <string>

namespace ApiTest::InstrumentPageApi
{

void runSource (SessionRig& rig)
{
    auto& session = rig.session();
    auto& client = rig.client();

    auto response = putWithDispatch (
        client,
        "/patches/0/fx",
        "{\"chain\":[{\"type\":\"compressor\",\"bypassed\":true}]}");
    check (response && response->status == 200, "the chain is prepared for the source change");
    check (chainLatency (parseJson (response->body)) == 0,
           "the bypassed preparation insert adds no latency");

    const double beforeSourceRms = rmsOf (capture (session, kSettleBlocks, kMeasureBlocks));

    check (beforeSourceRms > 0.01, "the slot renders before the source change");

    response = postWithDispatch (client, "/patches/0/source",
                                 "{\"kind\":\"init-tone\",\"name\":\"Test Tone\","
                                 "\"frequency\":880}");
    check (response && response->status == 200, "POST /patches/0/source returns 200");

    check (session.waitForSlotWarm (0, 5000),
           "the armed slot warms the new source");

    const auto sourceState = response ? parseJson (response->body) : juce::var {};
    const auto sourcePatch = patchForSlot (sourceState, 0);
    const auto instrument = property (sourcePatch, "instrument");

    check (property (instrument, "kind").toString() == "init-tone",
           "/state publishes the new source kind");
    check (property (instrument, "name").toString() == "Test Tone",
           "/state publishes the new source name");

    const auto tone = capture (session, kSettleBlocks, kMeasureBlocks);
    const double toneRms = rmsOf (tone);
    const double frequency = estimateFrequency (tone);

    check (toneRms > 0.01, "the new source renders non-silent audio");
    check (std::abs (frequency - 880.0) < 25.0,
           "the 880 Hz source is audible (measured " + std::to_string (frequency) + " Hz)");

    response = client.Get ("/patches/0/fx");
    check (response && insertCount (parseJson (response->body)) == 1
               && insertType (parseJson (response->body), 0) == "compressor",
           "the insert chain survives a source change");

    response = postWithDispatch (client, "/patches/0/source", "{}");
    check (response && response->status == 400, "a source without a kind is rejected");

    response = postWithDispatch (client, "/patches/0/source", "{\"kind\":\"chorus\"}");
    check (response && response->status == 400, "an unknown source kind is rejected");

    response = postWithDispatch (client, "/patches/0/source",
                                 "{\"kind\":\"plugin\",\"path\":\"/nonexistent.vst3\"}");
    check (response && response->status == 400, "a missing plug-in bundle is rejected");

    response = postWithDispatch (client, "/patches/0/source",
                                 "{\"kind\":\"init-tone\",\"frequency\":5}");
    check (response && response->status == 400, "a frequency outside the tone range is rejected");

    response = postWithDispatch (client, "/patches/0/source",
                                 "{\"kind\":\"init-tone\",\"bogus\":1}");
    check (response && response->status == 400, "an unknown source field is rejected");

    response = postWithDispatch (client, "/patches/99/source", "{\"kind\":\"init-tone\"}");
    check (response && response->status == 404, "an unknown slot is rejected");
}

} // namespace ApiTest::InstrumentPageApi
