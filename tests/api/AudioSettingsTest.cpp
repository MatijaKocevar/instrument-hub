// T053 acceptance test: audio settings routes plus the real device bridge.
//
// The first half starts the control server against an in-memory audio backend
// and proves the request/response contract: GET /audio, POST /audio (buffer
// size and channel mapping), validation and POST /audio/measure.
//
// The second half drives AudioSettingsBridge against the ALSA Loopback card:
// open 48 kHz / 128, change the buffer to 256 and back, read the latency
// budget, remap a channel, and measure a plausible round trip.

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
