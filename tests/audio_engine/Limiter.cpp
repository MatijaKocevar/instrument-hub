#include "Limiter.h"
#include "Fixtures.h"

#include <iostream>
#include <memory>

namespace AudioEngineTest
{

void runMasterLimiter()
{
    std::cout << "\n-- master limiter --" << std::endl;

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 256;

    AudioEngine::AudioEngine engine (2, 2);

    auto low = std::make_unique<SineSource> (110.0, 0.8f);
    auto high = std::make_unique<SineSource> (157.0, 0.8f);

    engine.getSlot (0).setSource (low.get());
    engine.getSlot (1).setSource (high.get());
    engine.getSlot (0).activate();
    engine.getSlot (1).activate();

    engine.prepare (sampleRate, blockSize, 2);

    juce::AudioBuffer<float> output (2, blockSize);
    juce::MidiBuffer midi;

    for (int i = 0; i < 8; ++i)
        engine.processBlock (output, midi);

    float outputPeak = 0.0f;
    float lowestGainReductionDb = 0.0f;

    for (int i = 0; i < 32; ++i)
    {
        engine.processBlock (output, midi);
        outputPeak = juce::jmax (outputPeak, output.getMagnitude (0, blockSize));
        lowestGainReductionDb = juce::jmin (
            lowestGainReductionDb,
            engine.getBus().getLimiter().getGainReductionDb());
    }

    const float thresholdGain = juce::Decibels::decibelsToGain (
        engine.getBus().getLimiter().getThresholdDb(), -100.0f);

    std::cout << "output peak " << outputPeak << ", threshold " << thresholdGain
              << ", gain reduction " << lowestGainReductionDb << " dB" << std::endl;

    check (outputPeak > 0.9f, "limiter test produces a hot signal");
    check (outputPeak <= thresholdGain + 1.0e-4f,
           "limiter keeps the output at or below the threshold");
    check (lowestGainReductionDb < -1.0f,
           "limiter reports meaningful gain reduction");
}

}
