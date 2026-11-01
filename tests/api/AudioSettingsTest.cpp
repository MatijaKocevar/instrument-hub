#include "TestSupport.h"
#include "audio_settings/DeviceBridge.h"
#include "audio_settings/Routes.h"

#include <iostream>

int main()
{
    ApiTest::AudioSettings::runRoutes();
    ApiTest::AudioSettings::runDeviceBridge();

    std::cout << (ApiTest::failures == 0 ? "audio-settings-test: PASS"
                                         : "audio-settings-test: FAILED")
              << " failures=" << ApiTest::failures << std::endl;

    return ApiTest::failures == 0 ? 0 : 1;
}
