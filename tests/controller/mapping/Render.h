#pragma once

#include "audio/Slot.h"
#include "audio/SlotSwitcher.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace MappingTest
{

struct SineSource final : public AudioEngine::SlotSource
{
    SineSource (double frequencyToUse, float amplitudeToUse, double phaseOffset);

    void prepare (double newSampleRate, int) override;
    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;

private:
    double frequency;
    float amplitude;
    double initialPhase;

    double phase = 0.0;
    double increment = 0.0;
};

struct RenderStats
{
    double minRms = 1.0e9;
    double maxDelta = 0.0;
    int blocks = 0;
    int silentBlocks = 0;
    bool alwaysRendered = true;
};

void renderBlock (AudioEngine::SlotSwitcher& switcher,
                  juce::MidiBuffer& midi,
                  RenderStats& stats,
                  float& previousSample);

}
