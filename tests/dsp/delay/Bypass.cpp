#include "Bypass.h"

#include "Fixtures.h"

namespace DspTest::Delay
{

void runBypass()
{
    std::cout << "\n-- bypass and 0% mix --" << std::endl;

    ihub::dsp::Delay delay;
    delay.prepareToPlay (48000.0, kBlockSize);
    configure (delay, ihub::dsp::DelayDivision::quarter, 120.0, 0.5f, 0.0f);

    juce::AudioBuffer<float> signal (2, 4096);

    for (int channel = 0; channel < signal.getNumChannels(); ++channel)
        for (int i = 0; i < signal.getNumSamples(); ++i)
            signal.setSample (channel, i,
                              std::sin (0.01f * static_cast<float> (i + channel)));

    juce::AudioBuffer<float> reference;
    reference.makeCopyOf (signal);

    processInBlocks (delay, signal);

    bool dryExact = true;

    for (int channel = 0; channel < signal.getNumChannels(); ++channel)
        for (int i = 0; i < signal.getNumSamples(); ++i)
            if (! juce::exactlyEqual (signal.getSample (channel, i),
                                      reference.getSample (channel, i)))
                dryExact = false;

    check (dryExact, "a 0% mix is a bit-exact dry passthrough");

    delay.setEnabled (false);
    signal.makeCopyOf (reference);
    processInBlocks (delay, signal);

    dryExact = true;

    for (int channel = 0; channel < signal.getNumChannels(); ++channel)
        for (int i = 0; i < signal.getNumSamples(); ++i)
            if (! juce::exactlyEqual (signal.getSample (channel, i),
                                      reference.getSample (channel, i)))
                dryExact = false;

    check (dryExact, "bypass is a bit-exact dry passthrough");

    delay.setEnabled (true);
}

}
