#include "TestSupport.h"
#include "core_api/Arm.h"
#include "core_api/Auth.h"
#include "core_api/Fixtures.h"
#include "core_api/Meters.h"
#include "core_api/RouteRegistration.h"
#include "core_api/Settings.h"
#include "core_api/State.h"
#include "core_api/Transport.h"

#include <iostream>

int main()
{
    ApiTest::CoreApi::Rig rig;
    rig.state.setSlotLatency (2, 240, 48000.0);

    if (! rig.start())
        return 1;

    ApiTest::CoreApi::runState (rig);
    ApiTest::CoreApi::runArm (rig);
    ApiTest::CoreApi::runSettings (rig);
    ApiTest::CoreApi::runTransport (rig);
    ApiTest::CoreApi::runRouteRegistration (rig);
    ApiTest::CoreApi::runAuth (rig);
    ApiTest::CoreApi::runMeters (rig);

    rig.server.stop();

    std::cout << (ApiTest::failures == 0 ? "api-test: PASS" : "api-test: FAILED")
              << " failures=" << ApiTest::failures << std::endl;

    return ApiTest::failures == 0 ? 0 : 1;
}
