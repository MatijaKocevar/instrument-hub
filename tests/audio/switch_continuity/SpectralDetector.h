#pragma once

#include <array>
#include <vector>

namespace AudioTest::SwitchContinuity
{

struct SpectralReport
{
    int framesAnalysed = 0;
    int detections = 0;
    double maxSplatterDb = -1000.0;
    int worstFrame = -1;
    std::array<int, 6> countsAbove {}; // -70, -60, -50, -40, -30, -20 dB
};

SpectralReport analyseSpectrum (const std::vector<float>& signal);

} // namespace AudioTest::SwitchContinuity
