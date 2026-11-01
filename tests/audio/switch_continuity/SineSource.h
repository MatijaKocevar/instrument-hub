#pragma once

#include "audio/Slot.h"

namespace AudioTest::SwitchContinuity
{

class SineSource final : public AudioEngine::SlotSource
{
public:
    SineSource (double frequencyToUse, float amplitudeToUse, double phaseOffset);

    void prepare (double newSampleRate, int) override;

    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;

private:
    double frequency;
    float amplitude;
    double initialPhase;

    double sampleRate = 48000.0;
    double phase = 0.0;
    double increment = 0.0;
};

}
