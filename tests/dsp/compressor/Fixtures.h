#pragma once

#include "../TestSupport.h"

#include "dsp/Compressor.h"

#include <juce_events/juce_events.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace DspTest::Compressor
{

using DspTest::check;
using DspTest::number;

constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 512;
constexpr double kStaticToleranceDb = 1.0;
constexpr double kToneFrequency = 1000.0;

double amplitudeForDb (double levelDb);

void fillTone (juce::AudioBuffer<float>& buffer, int channel,
               double levelDb, double frequency, double sampleRate);

void processInBlocks (ihub::dsp::Compressor& compressor,
                      juce::AudioBuffer<float>& buffer,
                      int blockSize);

double rmsDb (const juce::AudioBuffer<float>& buffer, int channel, int from, int to);

double measureSteadyGainDb (ihub::dsp::Compressor& compressor,
                            double levelDb,
                            double frequency = kToneFrequency,
                            double sampleRate = kSampleRate,
                            double seconds = 1.5);

}
