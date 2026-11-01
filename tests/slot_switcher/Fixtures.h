#pragma once

#include "audio/Slot.h"

#include <string>

namespace SlotSwitcherTest
{

extern int failures;

constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 128;
constexpr int kNumChannels = 2;
constexpr float kFadeMilliseconds = 3.0f;
constexpr int kFadeSamples = 144;

constexpr double kFrequency = 220.0;
constexpr float kAmplitude = 0.5f;

constexpr int kPreRollBlocks = 2;
constexpr int kSwitchBlock = 12;
constexpr int kRecordedBlocks = 24;

constexpr int kPreRollSamples = kPreRollBlocks * kBlockSize;
constexpr int kSwitchSample = kSwitchBlock * kBlockSize;
constexpr int kRecordedSamples = kRecordedBlocks * kBlockSize;

void check (bool condition, const std::string& what);

class SineSource final : public AudioEngine::SlotSource
{
public:
    SineSource (float amplitudeToUse, double frequencyToUse, double phaseOffset);

    void prepare (double newSampleRate, int) override;
    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;

    double getPhase() const noexcept { return phase; }

private:
    float amplitude;
    double frequency;
    double initialPhase;

    double sampleRate = 48000.0;
    double phase = 0.0;
    double increment = 0.0;
};

}
