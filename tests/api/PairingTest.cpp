#include "TestSupport.h"
#include "pairing/LanOff.h"
#include "pairing/Pairing.h"
#include "pairing/StartupFlag.h"

#include <iostream>

int main()
{
    ApiTest::Pairing::Fixture fixture;

    if (! fixture.start())
        return 1;

    ApiTest::Pairing::runLanOff (fixture);
    ApiTest::Pairing::runPairing (fixture);
    ApiTest::Pairing::runStartupFlag (fixture);

    std::cout << (ApiTest::failures == 0 ? "pairing-test: PASS" : "pairing-test: FAILED")
              << " failures=" << ApiTest::failures << std::endl;

    return ApiTest::failures == 0 ? 0 : 1;
}
