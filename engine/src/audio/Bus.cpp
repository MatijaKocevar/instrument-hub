#include "Bus.h"

#include <algorithm>
#include <cmath>

namespace AudioEngine
{

//==============================================================================
void MasterLimiter::prepare (double sampleRate,
                             float thresholdDb,
                             float releaseMilliseconds) noexcept
{
    gain = 1.0f;
    setThresholdDb (thresholdDb);

    const float releaseSamples = juce::jmax (
        1.0f,
        releaseMilliseconds * 0.001f * static_cast<float> (juce::jmax (1.0, sampleRate)));

    // One-pole release towards unity: the gain recovers by ~63% per release
    // time after a peak.
    releaseCoefficient = 1.0f - std::exp (-1.0f / releaseSamples);
}

void MasterLimiter::reset() noexcept
{
    gain = 1.0f;
}

void MasterLimiter::setThresholdDb (float thresholdDb) noexcept
{
    thresholdInDb = thresholdDb;
    threshold = juce::Decibels::decibelsToGain (thresholdDb, -100.0f);

    if (gain > 1.0f)
        gain = 1.0f;
}

void MasterLimiter::process (juce::AudioBuffer<float>& buffer, int numSamples) noexcept
{
    const int samples = juce::jmin (numSamples, buffer.getNumSamples());
    const int channels = buffer.getNumChannels();

    for (int i = 0; i < samples; ++i)
    {
        float peak = 0.0f;

        for (int channel = 0; channel < channels; ++channel)
            peak = juce::jmax (peak, std::abs (buffer.getReadPointer (channel)[i]));

        if (peak > threshold)
        {
            // Instantaneous attack: never let this sample through above the
            // threshold. Keep an existing, lower gain.
            const float requiredGain = threshold / peak;

            if (requiredGain < gain)
                gain = requiredGain;
        }
        else if (gain < 1.0f)
        {
            gain += releaseCoefficient * (1.0f - gain);

            if (gain > 1.0f || 1.0f - gain < 1.0e-6f)
                gain = 1.0f;
        }

        if (gain < 1.0f)
        {
            for (int channel = 0; channel < channels; ++channel)
                buffer.getWritePointer (channel)[i] *= gain;
        }
    }
}

float MasterLimiter::getGainReductionDb() const noexcept
{
    if (gain <= 0.0f)
        return -100.0f;

    return 20.0f * std::log10 (gain);
}

//==============================================================================
void Bus::prepare (double newSampleRate, int newMaximumBlockSize, int newNumChannels)
{
    jassert (newSampleRate > 0.0);
    jassert (newMaximumBlockSize > 0);
    jassert (newNumChannels > 0);

    sampleRate = newSampleRate;
    maximumBlockSize = juce::jmax (1, newMaximumBlockSize);
    numChannels = juce::jmax (1, newNumChannels);

    mixBuffer.setSize (numChannels, maximumBlockSize, false, true, true);
    mixBuffer.clear();

    limiter.prepare (sampleRate);
}

void Bus::releaseResources()
{
    mixBuffer.setSize (0, 0);
    limiter.reset();
}

void Bus::reset() noexcept
{
    mixBuffer.clear();
    limiter.reset();
}

void Bus::beginBlock (int numSamples) noexcept
{
    const int samples = juce::jmin (numSamples, mixBuffer.getNumSamples());

    if (samples > 0)
        mixBuffer.clear (0, samples);
}

void Bus::add (const juce::AudioBuffer<float>& source, int numSamples) noexcept
{
    const int samples = juce::jmin (numSamples,
                                    mixBuffer.getNumSamples(),
                                    source.getNumSamples());
    const int channels = juce::jmin (numChannels, source.getNumChannels());

    if (samples <= 0)
        return;

    for (int channel = 0; channel < channels; ++channel)
        mixBuffer.addFrom (channel, 0, source, channel, 0, samples);
}

void Bus::finish (juce::AudioBuffer<float>& destination, int numSamples) noexcept
{
    const int samples = juce::jmin (numSamples, mixBuffer.getNumSamples());

    if (samples <= 0)
    {
        destination.clear();
        return;
    }

    limiter.process (mixBuffer, samples);

    const int destinationChannels = destination.getNumChannels();
    const int copyChannels = juce::jmin (numChannels, destinationChannels);

    for (int channel = 0; channel < copyChannels; ++channel)
        destination.copyFrom (channel, 0, mixBuffer, channel, 0, samples);

    // Never leave stale audio in channels or samples the bus does not cover.
    for (int channel = copyChannels; channel < destinationChannels; ++channel)
        destination.clear (channel, 0, destination.getNumSamples());

    for (int channel = 0; channel < copyChannels; ++channel)
        if (destination.getNumSamples() > samples)
            destination.clear (channel, samples, destination.getNumSamples() - samples);
}

} // namespace AudioEngine
