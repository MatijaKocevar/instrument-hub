// T115 acceptance test: the control API drives a real, headless session.
//
// Composes the default session offline (no audio device), attaches the HTTP +
// WebSocket server through Api::attachSession() and proves:
//
//   - POST /arm 0 then 1 changes the audible slot: the rendered tone moves
//     from slot 0's 110 Hz to slot 1's 123.47 Hz, the crossfade gains swap and
//     the previous slot goes idle;
//   - /meters reports a non-zero peak for the playing slot and zero for the
//     idle one, sampled from the real slot outputs and the master bus;
//   - GET /state reports the actual slot state/gain, not a synthetic mirror;
//   - POST /transport start/stop drives the session transport and its
//     sample-accurate position, while a stop does not rewind it.
//
// Everything is offline and deterministic: rendering is driven by this test,
// so the result does not depend on wall-clock timing or a sound card.

#include "TestSupport.h"
#include "session_api/Arm.h"
#include "session_api/ArmRejection.h"
#include "session_api/Meters.h"
#include "session_api/PatchMix.h"
#include "session_api/Transport.h"

#include <juce_events/juce_events.h>

#include <iostream>

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    ApiTest::SessionRig rig (ApiTest::makeToneConfig (ApiTest::SessionApi::kSlots),
                             5,
                             "session-api-test");

    if (! rig.started())
        return 1;

    std::cout << "session-api-test: listening on 127.0.0.1:" << rig.port()
              << " slots=" << rig.session().getNumSlots() << std::endl;

    ApiTest::SessionApi::runArm (rig);
    ApiTest::SessionApi::runPatchMix (rig);
    ApiTest::SessionApi::runMeters (rig);
    ApiTest::SessionApi::runTransport (rig);
    ApiTest::SessionApi::runArmRejection (rig);

    rig.stop();

    std::cout << (ApiTest::failures == 0 ? "session-api-test: PASS"
                                         : "session-api-test: FAILED")
              << " failures=" << ApiTest::failures << std::endl;

    return ApiTest::failures == 0 ? 0 : 1;
}
