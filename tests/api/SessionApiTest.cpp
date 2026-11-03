#include "TestSupport.h"
#include "session_api/Arm.h"
#include "session_api/ArmRejection.h"
#include "session_api/Meters.h"
#include "session_api/PatchMix.h"
#include "session_api/Tracks.h"
#include "session_api/Transport.h"

#include <juce_events/juce_events.h>

#include <iostream>

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    {
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
    }

    ApiTest::SessionApi::runTracks();

    std::cout << (ApiTest::failures == 0 ? "session-api-test: PASS"
                                         : "session-api-test: FAILED")
              << " failures=" << ApiTest::failures << std::endl;

    return ApiTest::failures == 0 ? 0 : 1;
}
