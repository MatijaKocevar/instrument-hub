#include "app/DiagnosticModes.h"

#include "audio/AudioEngine.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

namespace ihub::app
{

// --measure-latency: measures the loopback round trip five times and reports
// the median, spread and whether it matches the 2 x buffer expectation.
int runLatencyMeasurement()
{
    constexpr int runs = 5;

    AudioEngine::AudioEngine engine;
    AudioEngine::LatencyProbeOptions options;
    options.bufferSize = 128;
    options.sampleRate = 48000.0;

    std::vector<int> samples;
    samples.reserve (runs);

    std::cout << "latency: measuring " << runs << " times at "
              << options.bufferSize << " frames / " << options.sampleRate << " Hz"
              << std::endl;

    for (int run = 1; run <= runs; ++run)
    {
        const auto result = engine.measureRoundTripLatency (options);

        if (! result.valid)
        {
            std::cerr << "latency: run " << run << " FAILED: "
                      << result.error.toRawUTF8() << std::endl;
            return 1;
        }

        samples.push_back (result.roundTripSamples);

        std::cout << "latency: run " << run << ": "
                  << result.roundTripSamples << " samples, "
                  << result.roundTripMs << " ms, confidence "
                  << result.confidence << std::endl;
    }

    std::sort (samples.begin(), samples.end());

    const int median = samples[samples.size() / 2];
    const int spread = samples.back() - samples.front();
    const double sampleRate = options.sampleRate;
    const double medianMs = static_cast<double> (median) * 1000.0 / sampleRate;
    const int expectedSamples = options.bufferSize * 2;
    const double expectedMs = static_cast<double> (expectedSamples) * 1000.0 / sampleRate;
    const int tolerance = options.bufferSize;

    const bool withinTolerance = std::abs (median - expectedSamples) <= tolerance;
    const bool consistent = spread <= tolerance;

    std::cout << "latency: median " << median << " samples (" << medianMs << " ms)"
              << " spread " << spread << " samples"
              << " expected " << expectedSamples << " samples (" << expectedMs << " ms)"
              << " tolerance +/-" << tolerance << " samples"
              << " within_tolerance=" << (withinTolerance ? "yes" : "no")
              << " consistent=" << (consistent ? "yes" : "no")
              << std::endl;

    return withinTolerance && consistent ? 0 : 1;
}

} // namespace ihub::app
