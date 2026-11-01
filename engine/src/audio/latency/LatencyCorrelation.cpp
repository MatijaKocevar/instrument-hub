#include "audio/LatencyProbe.h"

#include <cmath>

namespace AudioEngine
{

//==============================================================================
int LatencyProbe::findLagSamples (const float* reference,
                                  int referenceSamples,
                                  const float* captured,
                                  int capturedSamples,
                                  int maxLag,
                                  float* peak)
{
    if (peak != nullptr)
        *peak = 0.0f;

    if (reference == nullptr || captured == nullptr
        || referenceSamples <= 0 || capturedSamples <= 0 || maxLag < 0)
        return -1;

    maxLag = juce::jmin (maxLag, capturedSamples - 1);

    int bestLag = -1;
    double bestScore = 0.0;

    for (int lag = 0; lag <= maxLag; ++lag)
    {
        const int first = juce::jmax (0, lag);
        const int last = juce::jmin (capturedSamples, referenceSamples + lag);

        if (first >= last)
            continue;

        double sum = 0.0;
        double referenceEnergy = 0.0;
        double capturedEnergy = 0.0;

        for (int i = first; i < last; ++i)
        {
            const double ref = static_cast<double> (reference[i - lag]);
            const double cap = static_cast<double> (captured[i]);

            sum += ref * cap;
            referenceEnergy += ref * ref;
            capturedEnergy += cap * cap;
        }

        if (referenceEnergy <= 0.0 || capturedEnergy <= 0.0)
            continue;

        const double score = std::abs (sum) / std::sqrt (referenceEnergy * capturedEnergy);

        if (score > bestScore)
        {
            bestScore = score;
            bestLag = lag;
        }
    }

    if (peak != nullptr)
        *peak = static_cast<float> (bestScore);

    return bestLag;
}

} // namespace AudioEngine
