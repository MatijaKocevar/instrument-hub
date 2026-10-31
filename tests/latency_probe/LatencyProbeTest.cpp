// ihub-latency-probe-test: cross-correlation unit test plus five real
// round-trip measurements through the ALSA Loopback card.
//
// Acceptance for T012:
//   * the measured round-trip at 128 frames is within +/- one buffer of
//     2 * bufferSize / sampleRate
//   * five consecutive runs agree with each other.

#include "audio/LatencyProbe.h"

#include <juce_events/juce_events.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace
{
int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

void testCrossCorrelation()
{
    constexpr int referenceSamples = 2048;
    constexpr int capturedSamples = 2304;
    constexpr int impulseOffset = 300;
    constexpr int trueLag = 77;

    std::vector<float> reference (referenceSamples, 0.0f);
    std::vector<float> captured (capturedSamples, 0.0f);

    reference[impulseOffset] = 0.5f;
    captured[impulseOffset + trueLag] = 0.4f;

    // A second, smaller peak far away must not win.
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

    // Silence must not report a lag.
    std::vector<float> silence (capturedSamples, 0.0f);
    check (AudioEngine::LatencyProbe::findLagSamples (reference.data(),
                                                      referenceSamples,
                                                      silence.data(),
                                                      capturedSamples,
                                                      512)
               < 0,
           "silence reports no lag");
}

void testLoopbackLatency()
{
    constexpr int runs = 5;
    constexpr int bufferSize = 128;
    constexpr double sampleRate = 48000.0;
    constexpr int expectedSamples = bufferSize * 2;
    constexpr int tolerance = bufferSize;

    AudioEngine::LatencyProbe probe;
    AudioEngine::LatencyProbeOptions options;
    options.bufferSize = bufferSize;
    options.sampleRate = sampleRate;

    std::vector<int> samples;
    samples.reserve (runs);

    for (int run = 1; run <= runs; ++run)
    {
        const auto result = probe.measure (options);

        if (! result.valid)
        {
            check (false, "run " + std::to_string (run) + " failed: "
                              + result.error.toStdString());
            return;
        }

        samples.push_back (result.roundTripSamples);

        std::cout << "run " << run << ": " << result.roundTripSamples
                  << " samples (" << result.roundTripMs << " ms), "
                  << "buffer=" << result.bufferSize
                  << " rate=" << result.sampleRate
                  << " device=\"" << result.outputDeviceName << "\""
                  << " confidence=" << result.confidence << std::endl;

        check (result.bufferSize == bufferSize,
               "run " + std::to_string (run) + " ran at 128 frames");
        check (std::abs (result.roundTripMs
                         - static_cast<double> (result.roundTripSamples) * 1000.0
                               / sampleRate)
                   < 0.001,
               "run " + std::to_string (run) + " reports ms from the same lag");
        check (std::abs (result.roundTripSamples - expectedSamples) <= tolerance,
               "run " + std::to_string (run) + " is within one buffer of 2 buffers");
        check (result.confidence > 0.1f,
               "run " + std::to_string (run) + " found a clear impulse");
    }

    const auto minmax = std::minmax_element (samples.begin(), samples.end());
    const int spread = *minmax.second - *minmax.first;

    std::cout << "spread: " << spread << " samples across " << runs << " runs"
              << std::endl;

    check (spread <= tolerance, "five runs agree within one buffer");
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    testCrossCorrelation();
    testLoopbackLatency();

    std::cout << (failures == 0 ? "PASS" : "FAIL")
              << ": ihub-latency-probe (" << failures << " failure(s))"
              << std::endl;

    return failures == 0 ? 0 : 1;
}
