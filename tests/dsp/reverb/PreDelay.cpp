#include "PreDelay.h"

#include "Fixtures.h"

namespace DspTest::Reverb
{

void runPreDelay()
{
    std::cout << "\n-- pre-delay --" << std::endl;

    ihub::dsp::Reverb reverb;
    reverb.prepareToPlay (kSampleRate, kBlockSize);
    reverb.setSettings (makeSettings (0.6f, 0.4f, 0.0f, 1.0f, 50.0f));

    const auto response = renderImpulse (reverb, 2.0);
    const auto preDelaySamples = static_cast<int> (0.05 * kSampleRate);

    float leaked = 0.0f;

    for (int i = 0; i < preDelaySamples; ++i)
        leaked = juce::jmax (leaked, std::abs (response.getSample (0, i)),
                             std::abs (response.getSample (1, i)));

    const auto after = rms (response, 0, preDelaySamples,
                            preDelaySamples + 4000);

    std::cout << "      50 ms pre-delay: leaked " << leaked
              << " before sample " << preDelaySamples
              << ", tail rms after: " << after << std::endl;

    check (leaked <= 0.0f,
           "the wet output is exactly silent before the pre-delay");
    check (after > 1.0e-6,
           "the tail starts after the pre-delay");
}

}
