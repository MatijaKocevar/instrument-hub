#pragma once

#include "audio/Slot.h"
#include "audio/SlotSwitcher.h"
#include "audio/WarmPool.h"
#include "plugins/PluginInstrument.h"

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace WarmPoolTest
{

extern int failures;

constexpr int kNumSlots = 8;
constexpr int kNeighbours = 2;
constexpr int kExpectedLimit = 1 + kNeighbours;
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 128;

void check (bool condition, const std::string& what);

struct InstanceCounter
{
    std::atomic<int> live { 0 };
    std::atomic<int> peak { 0 };
    std::atomic<int> violations { 0 };
    int limit = kExpectedLimit;

    void noteCreated();
    void noteDestroyed();
    void resetPeak();
};

struct CountedInstance final : AudioEngine::SlotSource
{
    CountedInstance (InstanceCounter& counterToUse,
                     std::unique_ptr<ihub::plugins::PluginInstrument> instrumentToOwn);

    ~CountedInstance() override;

    void prepare (double sampleRate, int maximumBlockSize) override;
    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    int latencySamples() const noexcept override;

    InstanceCounter& counter;
    std::unique_ptr<ihub::plugins::PluginInstrument> instrument;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CountedInstance)
};

double toMilliseconds (std::chrono::steady_clock::duration duration);

juce::PluginDescription* findTestPlugin (juce::AudioPluginFormatManager& manager,
                                         const juce::File& bundle,
                                         juce::OwnedArray<juce::PluginDescription>& types);

bool contains (const std::vector<int>& values, int value);

bool pumpUntil (const std::function<bool()>& predicate, int timeoutMs = 30000);

std::string join (const std::vector<int>& values);

}
