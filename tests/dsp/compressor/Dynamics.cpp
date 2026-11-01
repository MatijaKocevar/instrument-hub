#include "Dynamics.h"

#include "Fixtures.h"

namespace DspTest::Compressor
{

void runDynamics()
{
    std::cout << "\n-- attack and release movement --" << std::endl;

    const auto renderAndReadGr = [] (float attackMs, float releaseMs,
                                     double toneSeconds, double silenceSeconds,
                                     float thresholdDb)
    {
        ihub::dsp::Compressor compressor;
        compressor.prepareToPlay (kSampleRate, kBlockSize);

        ihub::dsp::CompressorSettings settings;
        settings.thresholdDb = thresholdDb;
        settings.ratio = 4.0f;
        settings.attackMs = attackMs;
        settings.releaseMs = releaseMs;
        compressor.setSettings (settings);

        const auto toneSamples = static_cast<int> (toneSeconds * kSampleRate);
        const auto silenceSamples = static_cast<int> (silenceSeconds * kSampleRate);

        juce::AudioBuffer<float> buffer (1, toneSamples + silenceSamples);
        buffer.clear();
        fillTone (buffer, 0, 0.0, kToneFrequency, kSampleRate);

        for (int i = toneSamples; i < buffer.getNumSamples(); ++i)
            buffer.setSample (0, i, 0.0f);

        processInBlocks (compressor, buffer, kBlockSize);

        return compressor.getGainReductionDb();
    };

    // Fast attack reaches more of the target in 3 ms than a slow one.
    const auto fastAttack = renderAndReadGr (1.0f, 100.0f, 0.003, 0.0, -18.0f);
    const auto slowAttack = renderAndReadGr (100.0f, 100.0f, 0.003, 0.0, -18.0f);

    std::cout << "      after 3 ms: attack 1 ms -> " << fastAttack
              << " dB, attack 100 ms -> " << slowAttack << " dB" << std::endl;

    check (fastAttack > slowAttack + 5.0,
           "a fast attack reduces gain sooner than a slow attack");
    check (fastAttack > 8.0,
           "a 1 ms attack is most of the way to the static curve after 3 ms");

    // A short release gives gain back sooner than a long one.
    const auto shortRelease = renderAndReadGr (1.0f, 50.0f, 0.2, 0.1, -18.0f);
    const auto longRelease = renderAndReadGr (1.0f, 2000.0f, 0.2, 0.1, -18.0f);

    std::cout << "      after a 100 ms gap: release 50 ms -> " << shortRelease
              << " dB, release 2000 ms -> " << longRelease << " dB" << std::endl;

    check (shortRelease < longRelease - 1.0,
           "a short release gives gain back sooner than a long release");

    // After a long silence the gain reduction is back to zero.
    const auto settled = renderAndReadGr (1.0f, 100.0f, 0.2, 1.5, -18.0f);
    check (settled <= 0.05f,
           "a long silence returns the gain reduction to 0");
}

} // namespace DspTest::Compressor
