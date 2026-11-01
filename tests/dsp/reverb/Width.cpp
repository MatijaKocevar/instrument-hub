#include "Width.h"

#include "Fixtures.h"

namespace DspTest::Reverb
{

void runWidth()
{
    std::cout << "\n-- stereo width --" << std::endl;

    ihub::dsp::Reverb mono;
    mono.prepareToPlay (kSampleRate, kBlockSize);
    mono.setSettings (makeSettings (0.6f, 0.4f, 0.0f, 1.0f));
    const auto monoResponse = renderImpulse (mono, 2.0);

    ihub::dsp::Reverb wide;
    wide.prepareToPlay (kSampleRate, kBlockSize);
    wide.setSettings (makeSettings (0.6f, 0.4f, 1.0f, 1.0f));
    const auto wideResponse = renderImpulse (wide, 2.0);

    float maxDifferenceNarrow = 0.0f;
    float maxDifferenceWide = 0.0f;

    for (int i = 0; i < monoResponse.getNumSamples(); ++i)
    {
        maxDifferenceNarrow = juce::jmax (maxDifferenceNarrow,
                                          std::abs (monoResponse.getSample (0, i)
                                                    - monoResponse.getSample (1, i)));
        maxDifferenceWide = juce::jmax (maxDifferenceWide,
                                        std::abs (wideResponse.getSample (0, i)
                                                  - wideResponse.getSample (1, i)));
    }

    std::cout << "      max L/R difference: width 0 " << maxDifferenceNarrow
              << ", width 1 " << maxDifferenceWide << std::endl;

    check (maxDifferenceNarrow < 1.0e-6f,
           "width 0 produces identical left and right channels");
    check (maxDifferenceWide > 1.0e-4f,
           "width 1 decorrelates the left and right channels");
}

}
