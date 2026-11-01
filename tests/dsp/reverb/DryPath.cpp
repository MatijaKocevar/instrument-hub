#include "DryPath.h"

#include "Fixtures.h"

namespace DspTest::Reverb
{

void runDryPath()
{
    std::cout << "\n-- dry path --" << std::endl;

    ihub::dsp::Reverb reverb;
    reverb.prepareToPlay (kSampleRate, kBlockSize);
    reverb.setSettings (makeSettings (0.6f, 0.5f, 1.0f, 0.0f));

    juce::AudioBuffer<float> signal (2, 4096);

    for (int channel = 0; channel < 2; ++channel)
        for (int i = 0; i < signal.getNumSamples(); ++i)
            signal.setSample (channel, i,
                              std::sin (0.013f * static_cast<float> (i + 3 * channel)));

    juce::AudioBuffer<float> reference;
    reference.makeCopyOf (signal);

    processInBlocks (reverb, signal);

    bool exact = true;

    for (int channel = 0; channel < 2; ++channel)
        for (int i = 0; i < signal.getNumSamples(); ++i)
            if (! juce::exactlyEqual (signal.getSample (channel, i),
                                      reference.getSample (channel, i)))
                exact = false;

    check (exact, "a 0% mix is a bit-exact dry passthrough");

    reverb.setEnabled (false);
    signal.makeCopyOf (reference);
    processInBlocks (reverb, signal);

    exact = true;

    for (int channel = 0; channel < 2; ++channel)
        for (int i = 0; i < signal.getNumSamples(); ++i)
            if (! juce::exactlyEqual (signal.getSample (channel, i),
                                      reference.getSample (channel, i)))
                exact = false;

    check (exact, "bypass is a bit-exact dry passthrough");
    check (reverb.getLatencySamples() == 0, "the reverb reports zero latency");
    check (reverb.getTailLengthSeconds() > 0.0, "the reverb reports a tail length");
}

}
