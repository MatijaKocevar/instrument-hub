#include "TestSupport.h"
#include "patch_fx_api/Bypass.h"
#include "patch_fx_api/Chain.h"
#include "patch_fx_api/HostedInsert.h"
#include "patch_fx_api/Replace.h"
#include "patch_fx_api/Validation.h"

#include <juce_events/juce_events.h>

#include <iostream>

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    auto config = ApiTest::makeToneConfig (ApiTest::PatchFxApi::kSlots);
    config.neighbourCount = 1;

    ApiTest::SessionRig rig (config, 10, "patch-fx-api-test");

    if (! rig.started())
        return 1;

    std::cout << "patch-fx-api-test: listening on 127.0.0.1:" << rig.port()
              << " slots=" << rig.session().getNumSlots() << std::endl;

    ApiTest::PatchFxApi::Context context;

    ApiTest::PatchFxApi::runChain (rig, context);
    ApiTest::PatchFxApi::runBypass (rig, context);
    ApiTest::PatchFxApi::runHostedInsert (rig);
    ApiTest::PatchFxApi::runReplace (rig, context);
    ApiTest::PatchFxApi::runValidation (rig);

    rig.stop();

    std::cout << (ApiTest::failures == 0 ? "patch-fx-api-test: PASS"
                                         : "patch-fx-api-test: FAILED")
              << " failures=" << ApiTest::failures << std::endl;

    return ApiTest::failures == 0 ? 0 : 1;
}
