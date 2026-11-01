#include "Flat.h"

#include "Fixtures.h"

namespace DspTest::Eq
{

void runFlat()
{
    std::cout << "\n-- a flat / bypassed EQ is a bit-exact passthrough --" << std::endl;

    ihub::dsp::Eq eq;
    eq.prepareToPlay (kSampleRate, kBlockSize);

    juce::AudioBuffer<float> signal (2, kBlockSize);

    for (int channel = 0; channel < signal.getNumChannels(); ++channel)
        for (int i = 0; i < signal.getNumSamples(); ++i)
            signal.setSample (channel, i,
                              static_cast<float> (((i * 37 + channel * 11) % 101) - 50)
                                  / 50.0f);

    juce::AudioBuffer<float> reference;
    reference.makeCopyOf (signal);

    juce::MidiBuffer midi;
    eq.processBlock (signal, midi);

    bool bitEqual = true;
    const auto bytes = static_cast<std::size_t> (kBlockSize) * sizeof (float);

    for (int channel = 0; channel < signal.getNumChannels(); ++channel)
        if (std::memcmp (signal.getReadPointer (channel),
                         reference.getReadPointer (channel),
                         bytes) != 0)
            bitEqual = false;

    check (bitEqual, "0 dB on all three bands leaves the buffer bit-identical");

    eq.setEnabled (false);
    signal.makeCopyOf (reference);
    eq.processBlock (signal, midi);

    bitEqual = true;

    for (int channel = 0; channel < signal.getNumChannels(); ++channel)
        if (std::memcmp (signal.getReadPointer (channel),
                         reference.getReadPointer (channel),
                         bytes) != 0)
            bitEqual = false;

    check (bitEqual, "global bypass leaves the buffer bit-identical");

    eq.setEnabled (true);
    check (eq.isEnabled(), "the EQ can be re-enabled");
}

} // namespace DspTest::Eq
