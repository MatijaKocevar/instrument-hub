#include "TestSupport.h"
#include "Fixtures.h"

#include <cmath>
#include <iostream>
#include <numbers>

namespace AudioInputTest
{

int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

double dbfs (double linear)
{
    if (linear <= 0.0)
        return -200.0;

    return 20.0 * std::log10 (linear);
}

void fillSine (juce::AudioBuffer<float>& buffer, int channel)
{
    auto* samples = buffer.getWritePointer (channel);

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const double phase = 2.0 * std::numbers::pi * kFrequency
                             * static_cast<double> (i) / kSampleRate;
        samples[i] = static_cast<float> (kAmplitude * std::sin (phase));
    }
}

}
