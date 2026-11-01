#pragma once

#include "audio/Slot.h"

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <string>
#include <vector>

namespace FxChainTest
{

extern int failures;

constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 128;
constexpr int kNumChannels = 2;
constexpr int kFadeBlocks = 8;

constexpr float kGainA = 0.5f;
constexpr float kSineLevelB = 0.1f;
constexpr float kGainB = 0.25f;
constexpr int kLatencySamples = 240;

void check (bool condition, const std::string& what);

bool buffersBitEqual (const juce::AudioBuffer<float>& a,
                      const juce::AudioBuffer<float>& b);

/** A deterministic source whose pattern is identical on every block. */
class PatternSource final : public AudioEngine::SlotSource
{
public:
    void prepare (double, int maximumBlockSize) override;
    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;

    int latencySamples() const noexcept override { return 24; }

    const std::vector<float>& getPattern() const noexcept { return pattern; }

private:
    std::vector<float> pattern;
};

juce::PluginDescription* findTestPlugin (juce::AudioPluginFormatManager& manager,
                                         const juce::File& bundle,
                                         juce::OwnedArray<juce::PluginDescription>& types);

void fillPattern (juce::AudioBuffer<float>& buffer);

} // namespace FxChainTest
