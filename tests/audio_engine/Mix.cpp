#include "Mix.h"
#include "Fixtures.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

namespace AudioEngineTest
{

void runMixAgainstReference()
{
    std::cout << "\n-- mix against manual reference --" << std::endl;

    constexpr double sampleRate = 48000.0;
    constexpr int maxBlock = 512;
    constexpr std::int64_t totalSamples = 96000;
    constexpr std::int64_t compareFrom = 4096;

    AudioEngine::AudioEngine engine (3, 2);

    std::vector<std::unique_ptr<SineSource>> sources;
    sources.push_back (std::make_unique<SineSource> (220.0, 0.20f, 0.0));
    sources.push_back (std::make_unique<SineSource> (330.0, 0.15f, 1.1));
    sources.push_back (std::make_unique<SineSource> (440.0, 0.10f, 2.3));

    const std::vector<float> gains { 1.0f, 0.8f, 0.5f };

    for (int index = 0; index < 3; ++index)
    {
        engine.getSlot (index).setSource (sources[static_cast<size_t> (index)].get());
        engine.getSlot (index).setGain (gains[static_cast<size_t> (index)]);
        engine.getSlot (index).activate();
    }

    engine.prepare (sampleRate, maxBlock, 2);

    juce::AudioBuffer<float> output (2, maxBlock);
    juce::MidiBuffer midi;

    double sumSquaredDifference = 0.0;
    double maxDifference = 0.0;
    double referencePeak = 0.0;
    std::int64_t compared = 0;
    std::int64_t rendered = 0;

    while (rendered < totalSamples)
    {
        const auto remaining = totalSamples - rendered;
        const int samples = static_cast<int> (std::min<std::int64_t> (maxBlock, remaining));

        engine.processBlock (output, midi, samples);

        for (int i = 0; i < samples; ++i)
        {
            const std::int64_t absolute = rendered + i;

            if (absolute < compareFrom)
                continue;

            const double reference = referenceSample (sources, gains, absolute);
            referencePeak = std::max (referencePeak, std::abs (reference));

            for (int channel = 0; channel < 2; ++channel)
            {
                const double difference = static_cast<double> (output.getSample (channel, i))
                                          - reference;

                sumSquaredDifference += difference * difference;
                maxDifference = std::max (maxDifference, std::abs (difference));
                ++compared;
            }
        }

        rendered += samples;
    }

    const double rmsDifference = compared > 0
                                     ? std::sqrt (sumSquaredDifference / static_cast<double> (compared))
                                     : 0.0;
    const double rmsDb = dbfs (rmsDifference);

    std::cout << "rendered " << rendered << " samples, compared " << compared
              << " samples from " << compareFrom
              << ", reference peak " << referencePeak
              << ", max difference " << maxDifference
              << ", RMS difference " << rmsDb << " dBFS" << std::endl;

    check (rendered == totalSamples, "rendered exactly 2 s at 48 kHz (96000 samples)");
    check (referencePeak > 0.3, "reference mix has signal (peak > 0.3)");
    check (rmsDb < -90.0, "mix matches the manual reference within -90 dBFS");
    check (maxDifference < 1.0e-4, "no individual sample differs by more than 1e-4");
    check (sources[0]->processCalls.load() > 0
               && sources[1]->processCalls.load() > 0
               && sources[2]->processCalls.load() > 0,
           "all three slots were processed");
}

}
