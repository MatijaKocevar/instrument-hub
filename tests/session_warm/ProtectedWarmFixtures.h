#pragma once

#include "Session.h"

#include <atomic>
#include <functional>
#include <string>
#include <vector>

namespace ProtectedWarmTest
{

extern int failures;

constexpr int kSlots = 8;
constexpr int kUnprotectedSlots = 16;
constexpr int kSwitches = 100;
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 128;

void check (bool condition, const std::string& what);

std::vector<int> slotRange (int count);

bool pumpUntil (const std::function<bool()>& predicate, int timeoutMs = 10000);

struct SourceCounters
{
    std::atomic<int> created { 0 };
    std::atomic<int> live { 0 };
};

class CountingSource final : public AudioEngine::SlotSource
{
public:
    explicit CountingSource (SourceCounters& countersToUse);

    ~CountingSource() override;

    void prepare (double sampleRate, int maximumBlockSize) override;

    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    int latencySamples() const noexcept override;

private:
    SourceCounters& counters;
};

ihub::SessionConfig makeConfig();

void runWarmPoolScenario();

void runSessionScenario();

void runEmptyTrackScenario();

}
