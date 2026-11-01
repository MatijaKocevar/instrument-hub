#include "Bypass.h"

#include "Fixtures.h"

namespace DspTest::Compressor
{

void runBypass()
{
    std::cout << "\n-- bypass keeps the reported latency --" << std::endl;

    constexpr int numSamples = 4096;
    constexpr int impulseIndex = 64;

    ihub::dsp::Compressor compressor;
    compressor.setLookaheadMs (5.0f);
    compressor.prepareToPlay (kSampleRate, kBlockSize);
    compressor.setSettings (ihub::dsp::CompressorSettings {});

    const auto latency = compressor.getLatencySamples();

    juce::AudioBuffer<float> signal (1, numSamples);

    for (int i = 0; i < numSamples; ++i)
        signal.setSample (0, i,
                          static_cast<float> (((i * 37) % 101) - 50) / 50.0f);

    signal.setSample (0, impulseIndex, 1.0f);

    juce::AudioBuffer<float> reference;
    reference.makeCopyOf (signal);

    compressor.setEnabled (false);
    processInBlocks (compressor, signal, kBlockSize);

    bool exact = true;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto expected = i >= latency ? reference.getSample (0, i - latency) : 0.0f;

        if (! juce::exactlyEqual (signal.getSample (0, i), expected))
            exact = false;
    }

    check (exact, "bypass is a bit-exact copy delayed by the reported lookahead");
    check (compressor.getLatencySamples() == latency && latency == 240,
           "bypass does not change the reported latency");
    check (std::abs (compressor.getGainReductionDb()) < 1.0e-9f,
           "bypass reports zero gain reduction");

    // Re-enabling compresses again.
    compressor.setEnabled (true);
    compressor.reset();

    const auto gainDb = measureSteadyGainDb (compressor, -12.0);
    check (std::abs (-gainDb - 4.5) <= 1.0,
           "re-enabling restores compression");
}

} // namespace DspTest::Compressor
