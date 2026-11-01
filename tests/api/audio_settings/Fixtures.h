#pragma once

#include "api/Server.h"

#include <algorithm>
#include <string>

namespace ApiTest::AudioSettings
{

void recalculate (Api::AudioStateSnapshot& audio);

Api::AudioStateSnapshot makeFakeAudio();

struct FakeBackend
{
    Api::AudioStateSnapshot audio = makeFakeAudio();
    int measurements = 0;

    bool apply (const Api::AudioSettingsPatch& patch, std::string& error);

    Api::AudioMeasurementState measure();

    void attach (Api::Server& server);
};

} // namespace ApiTest::AudioSettings
