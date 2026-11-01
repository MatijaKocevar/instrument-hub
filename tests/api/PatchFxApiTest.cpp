// T121 acceptance test: the per-patch FX chain API drives the real audio path.
//
// Builds a small headless session, attaches the HTTP control server, arms a
// patch and then edits its insert chain entirely through the API:
//
//   - add a compressor (with look-ahead latency) and an EQ, and prove the
//     rendered audio changes;
//   - reorder the two inserts and prove the rendered audio changes again
//     (a compressor is non-linear, so it does not commute with the EQ);
//   - bypass an insert and watch its latency disappear from the chain total;
//   - set the wet/dry balance to zero and back;
//   - add, inspect and remove a hosted VST3 insert;
//   - replace the whole chain with PUT and clear it again;
//   - check the error statuses for bad slots, indices and fields.
//
// Everything is offline and deterministic: this test drives the render calls,
// so the result does not depend on a sound card or wall-clock timing.

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
