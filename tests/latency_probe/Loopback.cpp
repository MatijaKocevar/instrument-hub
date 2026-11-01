#include "Loopback.h"
#include "TestSupport.h"

#include "audio/LatencyProbe.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace LatencyProbeTest
{

void runLoopbackLatency()
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

}
