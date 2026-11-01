#include "TestSupport.h"

#include <cmath>
#include <iostream>

namespace SandboxTest
{

int failures = 0;

void expect (bool condition, const juce::String& description)
{
    if (condition)
    {
        std::cout << "ok:   " << description << std::endl;
    }
    else
    {
        ++failures;
        std::cerr << "FAIL: " << description << std::endl;
    }
}

float rmsOf (const juce::AudioBuffer<float>& buffer)
{
    double sum = 0.0;
    int count = 0;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const auto* samples = buffer.getReadPointer (channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto value = static_cast<double> (samples[sample]);
            sum += value * value;
            ++count;
        }
    }

    return count > 0 ? static_cast<float> (std::sqrt (sum / static_cast<double> (count))) : 0.0f;
}

void SteadySineSource::prepare (double sampleRate, int)
{
    increment = juce::MathConstants<double>::twoPi * 220.0 / sampleRate;
    phase = 0.0;
}

void SteadySineSource::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const auto amplitude = 0.2f;

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto value = amplitude * static_cast<float> (std::sin (phase));
        phase += increment;

        if (phase >= juce::MathConstants<double>::twoPi)
            phase -= juce::MathConstants<double>::twoPi;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            buffer.setSample (channel, sample, value);
    }
}

ihub::plugins::SandboxHost::Options makeFakeOptions (bool abortOnPurpose)
{
    ihub::plugins::SandboxHost::Options options;
    options.helperExecutable = juce::File (IHUB_SANDBOX_HELPER);
    options.sampleRate = 48000.0;
    options.maxBlockSize = 128;
    options.numChannels = 2;
    options.startupTimeoutMs = 15000;
    options.hangTimeoutMs = 1500;

    options.extraArguments.add ("--fake-tone");
    options.extraArguments.add ("440");

    if (abortOnPurpose)
    {
        options.extraArguments.add ("--abort-after");
        options.extraArguments.add ("3");
    }

    return options;
}

} // namespace SandboxTest
