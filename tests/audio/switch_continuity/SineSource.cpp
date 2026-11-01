#include "SineSource.h"

#include <cmath>

namespace AudioTest::SwitchContinuity
{

SineSource::SineSource (double frequencyToUse, float amplitudeToUse, double phaseOffset)
    : frequency (frequencyToUse),
      amplitude (amplitudeToUse),
      initialPhase (phaseOffset)
{
}

void SineSource::prepare (double newSampleRate, int)
{
    sampleRate = newSampleRate;
    phase = initialPhase;
    increment = juce::MathConstants<double>::twoPi * frequency / sampleRate;
}

void SineSource::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    for (int frame = 0; frame < buffer.getNumSamples(); ++frame)
    {
        const auto value = static_cast<float> (amplitude * std::sin (phase));
        phase += increment;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            buffer.getWritePointer (channel)[frame] = value;
    }
}

}
