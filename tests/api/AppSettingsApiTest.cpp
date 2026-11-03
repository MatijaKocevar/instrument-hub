#include "TestSupport.h"
#include "app_settings_api/Account.h"
#include "app_settings_api/Mixer.h"

#include <iostream>

int main()
{
    ApiTest::AppSettings::runMixerPersistence();
    ApiTest::AppSettings::runAccountPersistence();

    std::cout << (ApiTest::failures == 0 ? "app-settings-api-test: PASS"
                                         : "app-settings-api-test: FAILED")
              << " failures=" << ApiTest::failures << std::endl;

    return ApiTest::failures == 0 ? 0 : 1;
}
