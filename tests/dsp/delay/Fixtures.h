#pragma once

#include "../TestSupport.h"

#include "dsp/Delay.h"

#include <juce_events/juce_events.h>

#include <cmath>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace DspTest::Delay
{

using DspTest::check;
using DspTest::number;

constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 256;

void processInBlocks (ihub::dsp::Delay& delay,
                      juce::AudioBuffer<float>& buffer,
                      int blockSize = kBlockSize);

juce::AudioBuffer<float> makeImpulse (int numChannels, int numSamples, int impulseIndex);

int peakIndex (const juce::AudioBuffer<float>& buffer, int channel,
               int from, int to);

int countNonZero (const juce::AudioBuffer<float>& buffer, int channel,
                  int from, int to, float threshold = 1.0e-6f);

void configure (ihub::dsp::Delay& delay,
                ihub::dsp::DelayDivision division,
                double bpm,
                float feedback = 0.0f,
                float mix = 1.0f);

int measureEcho (ihub::dsp::Delay& delay, int impulseIndex, int numSamples);

}
