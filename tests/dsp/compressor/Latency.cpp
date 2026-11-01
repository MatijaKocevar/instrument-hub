#include "Latency.h"

#include "Fixtures.h"

#include <cmath>

namespace DspTest::Compressor
{

int measureImpulseDelay (ihub::dsp::Compressor& compressor,
                         int impulseIndex)
{
    const auto numSamples = impulseIndex + 4096;

    juce::AudioBuffer<float> buffer (1, numSamples);
    buffer.clear();
    buffer.setSample (0, impulseIndex, 1.0f);

    processInBlocks (compressor, buffer, kBlockSize);

    int peakIndex = 0;
    float peak = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto magnitude = std::abs (buffer.getSample (0, i));

        if (magnitude > peak)
        {
            peak = magnitude;
            peakIndex = i;
        }
    }

    for (int i = 0; i < numSamples; ++i)
        if (i != peakIndex && std::abs (buffer.getSample (0, i)) > 1.0e-6f)
            return -1000 - i;

    if (peak <= 0.0f)
        return -1;

    return peakIndex - impulseIndex;
}

void runLatency()
{
    std::cout << "\n-- reported latency vs measured impulse delay --" << std::endl;

    constexpr double sampleRates[] { 44100.0, 48000.0, 96000.0 };
    constexpr float lookaheads[] { 0.0f, 1.0f, 2.5f, 5.0f, 10.0f, 20.0f };

    for (const auto sampleRate : sampleRates)
    {
        for (const auto lookahead : lookaheads)
        {
            ihub::dsp::Compressor compressor;
            compressor.setLookaheadMs (lookahead);
            compressor.prepareToPlay (sampleRate, kBlockSize);

            const auto expected =
                static_cast<int> (std::lround (static_cast<double> (lookahead)
                                               * 0.001 * sampleRate));

            const auto measured = measureImpulseDelay (compressor, 100);

            const auto label = number (lookahead) + " ms at "
                               + std::to_string (static_cast<int> (sampleRate)) + " Hz";

            check (compressor.getLookaheadSamples() == expected,
                   "lookahead samples for " + label + " are "
                       + std::to_string (expected));
            check (compressor.getLatencySamples() == expected,
                   "reported latency for " + label + " is " + std::to_string (expected)
                       + " samples");
            check (measured == expected,
                   "measured delay for " + label + " is " + std::to_string (measured)
                       + " samples");
        }
    }

    ihub::dsp::Compressor compressor;
    compressor.setLookaheadMs (5.0f);
    compressor.prepareToPlay (kSampleRate, kBlockSize);

    juce::AudioBuffer<float> stereo (2, 8192);
    stereo.clear();
    stereo.setSample (0, 100, 0.5f);
    stereo.setSample (1, 100, -0.25f);
    processInBlocks (compressor, stereo, kBlockSize);

    const auto expected = compressor.getLatencySamples();
    check (std::abs (stereo.getSample (0, 100 + expected)) > 0.05f,
           "left impulse arrives at the reported latency");
    check (std::abs (stereo.getSample (1, 100 + expected)) > 0.02f,
           "right impulse arrives at the same reported latency");
    check (compressor.getLatencySamples() == 240,
           "5 ms at 48 kHz reports 240 samples");
}

}
