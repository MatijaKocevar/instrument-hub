#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace SamplerTest
{

double estimateFrequency (const juce::AudioBuffer<float>& buffer,
                          double sampleRate,
                          int channel = 0);

double meanOfMiddle (const juce::AudioBuffer<float>& buffer, int channel = 0);

}
