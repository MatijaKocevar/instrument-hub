#include "Fixtures.h"

#include <cmath>
#include <iostream>

namespace AudioEngineTest
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

SineSource::SineSource (double frequencyToUse, float amplitudeToUse, double phaseToUse)
    : frequency (frequencyToUse),
      amplitude (amplitudeToUse),
      phase (phaseToUse)
{
}

void SineSource::prepare (double newSampleRate, int)
{
    sampleRate = newSampleRate;
    position = 0;
}

void SineSource::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    processCalls.fetch_add (1, std::memory_order_relaxed);

    const int samples = buffer.getNumSamples();
    const int channels = buffer.getNumChannels();

    for (int i = 0; i < samples; ++i)
    {
        const float sample = static_cast<float> (valueAt (position + i));

        for (int channel = 0; channel < channels; ++channel)
            buffer.getWritePointer (channel)[i] = sample;
    }

    position += samples;
}

double SineSource::referenceAt (std::int64_t index) const
{
    return valueAt (index);
}

double SineSource::valueAt (std::int64_t index) const
{
    if (sampleRate <= 0.0)
        return 0.0;

    const double phaseNow = phase
                            + juce::MathConstants<double>::twoPi * frequency
                                  * static_cast<double> (index) / sampleRate;

    return static_cast<double> (amplitude) * std::sin (phaseNow);
}

double referenceSample (const std::vector<std::unique_ptr<SineSource>>& sources,
                        const std::vector<float>& gains,
                        std::int64_t index)
{
    double sum = 0.0;

    for (size_t i = 0; i < sources.size(); ++i)
        sum += static_cast<double> (gains[i]) * sources[i]->referenceAt (index);

    return sum;
}

}
