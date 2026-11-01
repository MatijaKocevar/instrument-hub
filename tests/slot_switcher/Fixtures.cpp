#include "Fixtures.h"

#include <cmath>
#include <iostream>

namespace SlotSwitcherTest
{

int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

SineSource::SineSource (float amplitudeToUse, double frequencyToUse, double phaseOffset)
    : amplitude (amplitudeToUse),
      frequency (frequencyToUse),
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

} // namespace SlotSwitcherTest
