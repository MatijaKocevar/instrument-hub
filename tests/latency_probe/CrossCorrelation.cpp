#include "CrossCorrelation.h"
#include "TestSupport.h"

#include "audio/LatencyProbe.h"

#include <vector>

namespace LatencyProbeTest
{

void runCrossCorrelation()
{
    constexpr int referenceSamples = 2048;
    constexpr int capturedSamples = 2304;
    constexpr int impulseOffset = 300;
    constexpr int trueLag = 77;

    std::vector<float> reference (referenceSamples, 0.0f);
    std::vector<float> captured (capturedSamples, 0.0f);

    reference[impulseOffset] = 0.5f;
    captured[impulseOffset + trueLag] = 0.4f;

    captured[impulseOffset + 800] = 0.05f;

    float confidence = 0.0f;
    const int lag = AudioEngine::LatencyProbe::findLagSamples (reference.data(),
                                                               referenceSamples,
                                                               captured.data(),
                                                               capturedSamples,
                                                               512,
                                                               &confidence);

    check (lag == trueLag, "cross-correlation finds the synthetic impulse lag");
    check (confidence > 0.99f, "synthetic impulse correlation is near 1");

    std::vector<float> silence (capturedSamples, 0.0f);
    check (AudioEngine::LatencyProbe::findLagSamples (reference.data(),
                                                      referenceSamples,
                                                      silence.data(),
                                                      capturedSamples,
                                                      512)
               < 0,
           "silence reports no lag");
}

}
