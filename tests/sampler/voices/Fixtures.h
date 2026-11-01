#pragma once

#include <juce_core/juce_core.h>

#include "sampler/KitLoader.h"
#include "sampler/SamplerInstrument.h"

namespace VoiceTest
{

struct Rig
{
    juce::File directory;
    ihub::sampler::LoadedKit kit;
    ihub::sampler::SamplerInstrument instrument;
};

std::string fixtureJson();

juce::AudioBuffer<float> render (ihub::sampler::SamplerInstrument& instrument,
                                 int numSamples);

float peakLevel (const juce::AudioBuffer<float>& buffer);
float peakLevel (const juce::AudioBuffer<float>& buffer, int start, int count);

}
