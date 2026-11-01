#include "Stability.h"

#include "Fixtures.h"

namespace DspTest::Reverb
{

void runStability()
{
    std::cout << "\n-- stability at maximum room size --" << std::endl;

    ihub::dsp::Reverb reverb;
    reverb.prepareToPlay (kSampleRate, kBlockSize);
    reverb.setSettings (makeSettings (1.0f, 0.0f, 1.0f, 1.0f));

    const auto seconds = 5;
    const auto numSamples = seconds * static_cast<int> (kSampleRate);

    juce::AudioBuffer<float> noise (2, numSamples);

    // Deterministic LCG so the test never depends on the standard library's
    // distribution details.
    std::uint32_t state = 0x12345678u;

    const auto next = [&state] () noexcept
    {
        state = state * 1664525u + 1013904223u;
        return (static_cast<float> (state) / 2147483648.0f) - 1.0f;
    };

    for (int channel = 0; channel < 2; ++channel)
        for (int i = 0; i < numSamples; ++i)
            noise.setSample (channel, i, 0.5f * next());

    processInBlocks (reverb, noise);

    float peak = 0.0f;
    bool finite = true;

    for (int channel = 0; channel < 2; ++channel)
        for (int i = 0; i < numSamples; ++i)
        {
            const auto value = noise.getSample (channel, i);
            finite = finite && std::isfinite (value);
            peak = juce::jmax (peak, std::abs (value));
        }

    std::cout << "      peak after 5 s of noise at room size 1.0: " << peak
              << std::endl;

    check (finite, "all output samples are finite");
    check (peak < 8.0f, "the network does not run away at maximum room size");

    // Silence in, silence out: the tail must eventually die. A room size of
    // 0.6 decays fast enough to be gone within the reported tail length; at
    // 1.0 the Freeverb feedback is deliberately very long.
    ihub::dsp::Reverb closing;
    closing.prepareToPlay (kSampleRate, kBlockSize);
    closing.setSettings (makeSettings (0.6f, 0.4f, 1.0f, 1.0f));

    juce::AudioBuffer<float> tail (2, static_cast<int> (1.0 * kSampleRate));
    tail.clear();
    tail.setSample (0, 0, 1.0f);
    tail.setSample (1, 0, 1.0f);
    processInBlocks (closing, tail);

    juce::AudioBuffer<float> silence (2, static_cast<int> (8.0 * kSampleRate));
    silence.clear();
    processInBlocks (closing, silence);

    const auto last = rms (silence, 0, silence.getNumSamples() - 4096,
                           silence.getNumSamples());

    std::cout << "      rms after 8 s of silence: " << last << std::endl;
    check (last < 1.0e-5, "the tail decays to silence");
}

} // namespace DspTest::Reverb
