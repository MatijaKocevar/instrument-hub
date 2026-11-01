#pragma once

#include "../TestSupport.h"

#include "dsp/Reverb.h"

#include <juce_events/juce_events.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace DspTest::Reverb
{

using DspTest::check;

constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 256;

void processInBlocks (ihub::dsp::Reverb& reverb,
                      juce::AudioBuffer<float>& buffer,
                      int blockSize = kBlockSize);

juce::AudioBuffer<float> renderImpulse (ihub::dsp::Reverb& reverb,
                                        double seconds,
                                        double sampleRate = kSampleRate);

double rms (const juce::AudioBuffer<float>& buffer, int channel, int from, int to);

double highFrequencyRatio (const juce::AudioBuffer<float>& buffer, int channel,
                           int from, int to);

int tailLengthSamples (const juce::AudioBuffer<float>& buffer, int channel,
                       float relativeThreshold = 1.0e-4f);

ihub::dsp::ReverbSettings makeSettings (float roomSize, float damping,
                                        float width, float mix,
                                        float preDelayMs = 0.0f);

} // namespace DspTest::Reverb
