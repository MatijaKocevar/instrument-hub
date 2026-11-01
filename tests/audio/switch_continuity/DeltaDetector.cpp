#include "DeltaDetector.h"

#include "Constants.h"

#include <cmath>

namespace AudioTest::SwitchContinuity
{

DeltaReport analyseDelta (const std::vector<float>& signal)
{
    DeltaReport report;

    for (std::size_t n = 1; n < signal.size(); ++n)
    {
        const double delta = std::abs (static_cast<double> (signal[n])
                                       - static_cast<double> (signal[n - 1]));

        if (delta > report.maxDelta)
        {
            report.maxDelta = delta;
            report.worstSample = static_cast<int> (n);
        }

        if (delta > kDeltaThreshold)
            ++report.detections;
    }

    return report;
}

}
