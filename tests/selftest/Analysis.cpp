#include "Analysis.h"
#include "Constants.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace SelfTest
{

double rms (const std::int16_t* samples, std::size_t count)
{
    if (count == 0)
        return 0.0;

    double sum = 0.0;

    for (std::size_t i = 0; i < count; ++i)
    {
        const double value = static_cast<double> (samples[i]);
        sum += value * value;
    }

    return std::sqrt (sum / static_cast<double> (count));
}

Report analyse (const std::vector<std::int16_t>& source,
                const std::vector<std::int16_t>& capture)
{
    Report report;
    report.firstSilentBlock = std::numeric_limits<unsigned int>::max();
    report.framesOk = capture.size() == kTotalFrames;
    report.sourceRms = rms (source.data(), source.size());
    report.captureRms = rms (capture.data(), capture.size());

    if (report.sourceRms > 0.0 && report.captureRms > 0.0)
        report.rmsDb = 20.0 * std::log10 (report.captureRms / report.sourceRms);
    else
        report.rmsDb = -std::numeric_limits<double>::infinity();

    report.rmsOk = std::abs (report.rmsDb) <= 1.0;

    const double gapThreshold = std::max (report.sourceRms * 0.01, 1.0);
    unsigned int currentRun = 0;
    const auto blocks = capture.size() / kPeriodFrames;

    for (std::size_t block = 0; block < blocks; ++block)
    {
        const double blockRms = rms (capture.data() + block * kPeriodFrames, kPeriodFrames);

        if (blockRms < gapThreshold)
        {
            ++report.silentBlocks;

            if (report.firstSilentBlock == std::numeric_limits<unsigned int>::max())
                report.firstSilentBlock = static_cast<unsigned int> (block);

            ++currentRun;
            report.longestSilentRun = std::max (report.longestSilentRun, currentRun);
        }
        else
        {
            currentRun = 0;
        }
    }

    report.noGap = report.silentBlocks == 0;
    return report;
}

}
