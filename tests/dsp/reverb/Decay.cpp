#include "Decay.h"

#include "Fixtures.h"

namespace DspTest::Reverb
{

void runDecay()
{
    std::cout << "\n-- decay and room size --" << std::endl;

    ihub::dsp::Reverb small;
    small.prepareToPlay (kSampleRate, kBlockSize);
    small.setSettings (makeSettings (0.2f, 0.4f, 0.0f, 1.0f));
    const auto smallResponse = renderImpulse (small, 6.0);

    ihub::dsp::Reverb large;
    large.prepareToPlay (kSampleRate, kBlockSize);
    large.setSettings (makeSettings (0.95f, 0.4f, 0.0f, 1.0f));
    const auto largeResponse = renderImpulse (large, 6.0);

    const auto window = static_cast<int> (0.1 * kSampleRate);

    const auto early = rms (largeResponse, 0, 0, 5 * window);
    const auto later = rms (largeResponse, 0, 30 * window, 35 * window);
    const auto latest = rms (largeResponse, 0, 45 * window, 50 * window);

    std::cout << "      rms windows (0-0.5 s, 3.0-3.5 s, 4.5-5.0 s): "
              << early << ", " << later << ", " << latest << std::endl;

    check (early > later && later > latest,
           "the reverb tail decays monotonically across 5 s");

    const auto smallTail = tailLengthSamples (smallResponse, 0);
    const auto largeTail = tailLengthSamples (largeResponse, 0);

    std::cout << "      tail length (-80 dB): small room " << smallTail
              << " samples, large room " << largeTail << " samples" << std::endl;

    check (smallTail > 0, "the small room has a non-empty tail");
    check (largeTail > smallTail,
           "a larger room size produces a longer tail");

    const auto tailSeconds = static_cast<double> (largeTail) / kSampleRate;
    check (tailSeconds < ihub::dsp::Reverb::maxTailSeconds,
           "the tail fits inside the reported tail length");

    // Every sample must be finite.
    bool finite = true;

    for (int channel = 0; channel < 2; ++channel)
        for (int i = 0; i < largeResponse.getNumSamples(); ++i)
            if (! std::isfinite (largeResponse.getSample (channel, i)))
                finite = false;

    check (finite, "the impulse response contains only finite samples");
}

} // namespace DspTest::Reverb
