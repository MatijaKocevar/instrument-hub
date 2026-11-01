#pragma once

#include "Constants.h"

#include <array>
#include <vector>

namespace AudioTest::SwitchContinuity
{

struct RunResult
{
    std::vector<float> master;
    std::vector<int> switchSamples;
    std::array<int, kNumSlots> switchesPerSlot {};
    int switchesIssued = 0;
    bool limiterStayedUnity = true;
    double peak = 0.0;
};

RunResult renderRun (float fadeMilliseconds);

} // namespace AudioTest::SwitchContinuity
