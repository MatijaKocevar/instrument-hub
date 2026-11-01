#pragma once

#include <cstdint>
#include <vector>

namespace SelfTest
{

struct Report
{
    bool framesOk = false;
    bool rmsOk = false;
    bool noGap = false;
    double sourceRms = 0.0;
    double captureRms = 0.0;
    double rmsDb = 0.0;
    unsigned int silentBlocks = 0;
    unsigned int longestSilentRun = 0;
    unsigned int firstSilentBlock = 0;
};

double rms (const std::int16_t* samples, std::size_t count);
Report analyse (const std::vector<std::int16_t>& source,
                const std::vector<std::int16_t>& capture);

}
