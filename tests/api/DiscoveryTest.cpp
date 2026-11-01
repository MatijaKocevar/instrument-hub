// T102 acceptance test: mDNS advertisement for `_instrumenthub._tcp`.
//
// 1. The DNS-SD type and host-label sanitising are correct.
// 2. Starting Discovery answers a PTR query on the mDNS multicast group with
//    the instance PTR plus SRV/TXT/A records carrying the API port.
// 3. Restarting on another port replaces the SRV port, and stop() ends the
//    advertisement.
//
// The test is a real multicast round trip, not a stubbed one. On a host with
// no working multicast route the integration half is skipped with a PASS note
// (the same graceful skip the acceptance recipe applies when avahi is absent).

#include "TestSupport.h"
#include "discovery/Advertisement.h"
#include "discovery/Helpers.h"

#include <iostream>

int main()
{
    ApiTest::Discovery::runPureHelpers();
    ApiTest::Discovery::runAdvertisement();

    std::cout << (ApiTest::failures == 0 ? "discovery-test: PASS"
                                         : "discovery-test: FAIL")
              << std::endl;

    return ApiTest::failures == 0 ? 0 : 1;
}
