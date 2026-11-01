#include "Replace.h"

#include "../AudioMeasurements.h"
#include "../FxChainJson.h"
#include "../JsonSupport.h"
#include "../RequestDispatch.h"
#include "../TestSupport.h"

#include <cmath>
#include <string>

namespace ApiTest::PatchFxApi
{

void runReplace (SessionRig& rig, const Context& context)
{
    auto& session = rig.session();
    auto& client = rig.client();

    const std::string replacement = R"({"chain":[
                                        {"type":"delay","wet":0.5,
                                         "parameters":{"tempoSync":0,"timeMs":50,
                                                       "feedback":0.5}}]})";
    auto response = putWithDispatch (client, "/patches/0/fx", replacement);
    check (response && response->status == 200, "PUT /patches/0/fx returns 200");

    if (response && response->status == 200)
    {
        const auto chain = parseJson (response->body);
        check (insertCount (chain) == 1 && insertType (chain, 0) == "delay",
               "PUT replaces the chain with the delay");
        check (std::abs (numberProperty ((*arrayProperty (chain, "inserts"))[0], "wet")
                         - 0.5) < 1.0e-9,
               "PUT carries the wet/dry balance");
        check (chainLatency (chain) == 0, "the delay reports no latency");
    }

    const auto delayed = capture (session, kSettleBlocks, kMeasureBlocks);
    check (maxDifference (context.baseline, delayed) > 1.0e-3,
           "the replacement chain changes the rendered audio");

    response = putWithDispatch (client, "/patches/0/fx", R"({"chain":[]})");
    check (response && response->status == 200, "PUT an empty chain returns 200");
    check (response && insertCount (parseJson (response->body)) == 0,
           "the chain is empty again");
    check (response && chainLatency (parseJson (response->body)) == 0,
           "the cleared chain reports zero latency");
}

} // namespace ApiTest::PatchFxApi
