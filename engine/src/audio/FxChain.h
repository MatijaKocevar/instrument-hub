#pragma once

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <atomic>
#include <vector>

namespace AudioEngine {

class FxChain {
public:
    FxChain() = default;
    ~FxChain() = default;

    void prepare(double sampleRate, int maximumBlockSize, int numChannels);

    void releaseResources();

    void reset() noexcept;

    int add(juce::AudioProcessor* processor);

    int insert(int index, juce::AudioProcessor* processor);

    bool remove(int index) noexcept;

    void clear() noexcept;

    int size() const noexcept { return static_cast<int>(processors.size()); }
    bool isEmpty() const noexcept { return processors.empty(); }

    juce::AudioProcessor* getProcessor(int index) const noexcept;
    int indexOf(const juce::AudioProcessor* processor) const noexcept;

    void refreshLatency() noexcept;

    int latencySamples() const noexcept { return cachedLatency.load(std::memory_order_relaxed); }

    void process(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) noexcept;

private:
    void prepareProcessor(juce::AudioProcessor& processor);

    std::vector<juce::AudioProcessor*> processors;

    double sampleRate = 48000.0;
    int maximumBlockSize = 0;
    int numChannels = 0;
    bool prepared = false;

    std::atomic<int> cachedLatency{0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FxChain)
};

}  // namespace AudioEngine
