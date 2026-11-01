#include "Chain.h"

#include "../AudioMeasurements.h"
#include "../FxChainJson.h"
#include "../JsonSupport.h"
#include "../TestSupport.h"

#include <cmath>
#include <iostream>

namespace ApiTest::PatchFxApi
{

void runChain (SessionRig& rig, Context& context)
{
    auto& session = rig.session();
    auto& client = rig.client();

    auto response = client.Post ("/arm", "{\"slot\":0}", "application/json");
    check (response && response->status == 200, "POST /arm 0 returns 200");
    check (session.waitForSlotActive (0, 5000),
           "slot 0 warms and becomes the active patch");

    renderBlocks (session, 16);

    context.baseline = capture (session, 0, kMeasureBlocks);
    const double baselineRms = rmsOf (context.baseline);
    check (baselineRms > 0.01, "the armed patch renders a non-silent baseline");

    response = client.Get ("/patches/0/fx");
    check (response && response->status == 200, "GET /patches/0/fx returns 200");

    if (response && response->status == 200)
    {
        const auto chain = parseJson (response->body);
        check (insertCount (chain) == 0, "a fresh patch has an empty insert chain");
        check (chainLatency (chain) == 0, "an empty chain reports zero latency");
    }

    response = client.Post ("/patches/0/fx",
                            R"({"type":"compressor","parameters":{
                                   "thresholdDb":-45,"ratio":10,
                                   "attackMs":1,"releaseMs":50}})",
                            "application/json");
    check (response && response->status == 200, "POST a compressor insert returns 200");

    int compressorLatency = 0;

    if (response && response->status == 200)
    {
        const auto chain = parseJson (response->body);
        compressorLatency = chainLatency (chain);
        check (insertCount (chain) == 1, "the compressor was added to the chain");
        check (insertType (chain, 0) == "compressor",
               "GET reports the compressor at index 0");
        check (compressorLatency == 240,
               "the compressor's 5 ms look-ahead is reported as 240 samples");
    }

    const auto compressed = capture (session, kSettleBlocks, kMeasureBlocks);
    const double compressedRms = rmsOf (compressed);
    std::cout << "patch-fx-api-test: baseline=" << dbfs (baselineRms)
              << " dBFS compressed=" << dbfs (compressedRms)
              << " dBFS latency=" << compressorLatency << std::endl;

    check (compressedRms < baselineRms * 0.5,
           "adding the compressor changes the rendered audio (level drops)");

    response = client.Post ("/patches/0/fx",
                            R"({"type":"eq","parameters":{
                                   "midFrequencyHz":110,"midGainDb":12,"midQ":1.0}})",
                            "application/json");
    check (response && response->status == 200, "POST an eq insert returns 200");
    check (response && insertCount (parseJson (response->body)) == 2,
           "the chain now has two inserts");

    const auto compressorFirst = capture (session, kSettleBlocks, kMeasureBlocks);
    const double compressorFirstRms = rmsOf (compressorFirst);

    response = client.Post ("/patches/0/fx/reorder", R"({"order":[1,0]})",
                            "application/json");
    check (response && response->status == 200, "POST /fx/reorder returns 200");

    if (response && response->status == 200)
    {
        const auto chain = parseJson (response->body);
        check (insertType (chain, 0) == "eq" && insertType (chain, 1) == "compressor",
               "reorder puts the eq before the compressor");
        check (chainLatency (chain) == 240,
               "reordering keeps the chain latency at 240 samples");
    }

    const auto eqFirst = capture (session, kSettleBlocks, kMeasureBlocks);
    const double eqFirstRms = rmsOf (eqFirst);
    context.eqFirstRms = eqFirstRms;
    std::cout << "patch-fx-api-test: compressor-first=" << dbfs (compressorFirstRms)
              << " dBFS eq-first=" << dbfs (eqFirstRms) << " dBFS" << std::endl;

    check (std::abs (dbfs (compressorFirstRms) - dbfs (eqFirstRms)) > 1.0,
           "reordering the chain changes the rendered audio");
    check (maxDifference (compressorFirst, eqFirst) > 1.0e-3,
           "the two orders produce different sample streams");
}

}
