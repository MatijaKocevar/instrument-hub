#pragma once

#include <vector>

namespace AudioTest::SwitchContinuity
{

struct DeltaReport
{
    int detections = 0;
    double maxDelta = 0.0;
    int worstSample = -1;
};

DeltaReport analyseDelta (const std::vector<float>& signal);

} // namespace AudioTest::SwitchContinuity
