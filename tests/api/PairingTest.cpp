// T101 acceptance test: LAN mode, 6-digit pairing codes and tokens.
//
// 1. With LAN mode off the loopback API needs no token.
// 2. Enabling LAN mode rebinds the listener, issues a 6-digit code and starts
//    answering unauthenticated requests with 401.
// 3. POST /pair with the code returns a token; that token grants /state.
// 4. Codes are single-use, tokens can be revoked, and turning LAN mode off
//    restores loopback-only unauthenticated access.

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
