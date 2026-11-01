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
