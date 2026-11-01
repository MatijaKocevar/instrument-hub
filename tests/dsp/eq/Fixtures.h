#pragma once

#include "../TestSupport.h"

#include "dsp/Eq.h"

#include <juce_events/juce_events.h>

#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace DspTest::Eq
{

using DspTest::check;

constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 512;
constexpr int kImpulseLength = 1 << 15;
constexpr double kToleranceDb = 0.5;

struct TargetBiquad
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0;
    double a1 = 0.0, a2 = 0.0;
};

TargetBiquad designTarget (ihub::dsp::EqBandType type,
                           double sampleRate,
                           double frequency,
                           double q,
                           double gainDb,
                           bool enabled);

double targetMagnitudeDb (const std::array<ihub::dsp::EqBandSettings,
                                           ihub::dsp::Eq::numBands>& bands,
                          double sampleRate,
                          double frequency);

double measureMagnitudeDb (ihub::dsp::Eq& eq, double sampleRate, double frequency);

std::array<ihub::dsp::EqBandSettings, ihub::dsp::Eq::numBands> makeTestBands();

} // namespace DspTest::Eq
