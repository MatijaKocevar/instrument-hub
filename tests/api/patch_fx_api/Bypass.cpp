#include "Bypass.h"

#include "../FxChainJson.h"
#include "../JsonSupport.h"
#include "../TestSupport.h"

namespace ApiTest::PatchFxApi
{

void runBypass (SessionRig& rig, const Context& context)
{
    auto& session = rig.session();
    auto& client = rig.client();

    auto response = client.Patch ("/patches/0/fx/1", R"({"bypassed":true})",
                                  "application/json");
    check (response && response->status == 200, "PATCH bypass returns 200");

    if (response && response->status == 200)
    {
        const auto chain = parseJson (response->body);
        check (chainLatency (chain) == 0,
               "a bypassed insert contributes no latency");
        check (static_cast<bool> (property ((*arrayProperty (chain, "inserts"))[1],
                                            "bypassed")),
               "GET reports the insert as bypassed");
    }

    const auto bypassed = capture (session, kSettleBlocks, kMeasureBlocks);
    check (rmsOf (bypassed) > context.eqFirstRms * 2.0,
           "bypassing the compressor restores the louder eq-only signal");

    response = client.Patch ("/patches/0/fx/1", R"({"bypassed":false})",
                             "application/json");
    check (response && response->status == 200, "PATCH unbypass returns 200");

    response = client.Patch ("/patches/0/fx/1", R"({"wet":0.0})",
                             "application/json");
    check (response && response->status == 200, "PATCH wet 0 returns 200");

    if (response && response->status == 200)
    {
        check (chainLatency (parseJson (response->body)) == 0,
               "a fully dry insert contributes no latency");
    }

    const auto dryOnly = capture (session, kSettleBlocks, kMeasureBlocks);
    check (rmsOf (dryOnly) > context.eqFirstRms * 2.0,
           "a fully dry compressor leaves the eq-only signal");

    response = client.Patch ("/patches/0/fx/1", R"({"wet":1.0})",
                             "application/json");
    check (response && response->status == 200, "PATCH wet 1 returns 200");

    response = client.Patch ("/patches/0/fx/0", R"({"position":1})",
                             "application/json");
    check (response && response->status == 200, "PATCH position returns 200");

    if (response && response->status == 200)
    {
        const auto chain = parseJson (response->body);
        check (insertType (chain, 0) == "compressor"
                   && insertType (chain, 1) == "eq",
               "PATCH position moves the eq after the compressor");
    }
}

} // namespace ApiTest::PatchFxApi
