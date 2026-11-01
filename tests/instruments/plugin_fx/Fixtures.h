#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <string>

namespace PluginFxTest
{

extern int failures;

constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 128;
constexpr int kNumChannels = 2;

constexpr float kGain = 0.5f;
constexpr float kLatencyMs = 5.0f;
constexpr int kLatencySamples = 240;

constexpr float kStateMarker = 0.875f;
constexpr float kStateGain = 0.25f;
constexpr float kStateFrequency = (1000.0f - 20.0f) / (20000.0f - 20.0f);

void check (bool condition, const std::string& what);
bool nearlyEqual (float a, float b, float tolerance = 1.0e-3f);

juce::PluginDescription* findTestPlugin (juce::AudioPluginFormatManager& manager,
                                         const juce::File& bundle,
                                         juce::OwnedArray<juce::PluginDescription>& types);

void fillPattern (juce::AudioBuffer<float>& buffer);
void pumpMessages (int milliseconds);

} // namespace PluginFxTest
