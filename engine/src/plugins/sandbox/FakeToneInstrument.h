#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace ihub::plugins::sandbox_client
{

class FakeToneInstrument
{
public:
    FakeToneInstrument (double sampleRateToUse, double frequency, int abortAfter);

    int latencySamples() const noexcept { return 0; }

    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi);

private:
    double increment;
    int abortAfterBlocks;
    double phase = 0.0;
    int blocksRendered = 0;
};

}
