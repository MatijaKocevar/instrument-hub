#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace ihub::sampler
{

struct SampleData
{
    juce::AudioBuffer<float> audio;

    double sampleRate = 0.0;

    double sourceSampleRate = 0.0;

    juce::int64 sourceFrames = 0;

    int sourceChannels = 0;

    juce::String name;
    juce::String path;

    int getNumChannels() const noexcept { return audio.getNumChannels(); }
    int getNumSamples() const noexcept { return audio.getNumSamples(); }
    bool isEmpty() const noexcept { return audio.getNumSamples() == 0 || audio.getNumChannels() == 0; }

    double getLengthSeconds() const noexcept
    {
        return sampleRate > 0.0 ? (double) audio.getNumSamples() / sampleRate : 0.0;
    }

    double getSourceLengthSeconds() const noexcept
    {
        return sourceSampleRate > 0.0 ? (double) sourceFrames / sourceSampleRate : 0.0;
    }
};

}
