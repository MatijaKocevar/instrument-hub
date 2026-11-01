#pragma once

#include "FxInsertTypes.h"

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <atomic>
#include <memory>
#include <string>

namespace AudioEngine
{

/**
    One effect in a patch's insert chain.

    It is a real `juce::AudioProcessor`, so `FxChain` stores it exactly like a
    hosted plugin. It owns the wrapped processor (a built-in DSP block or a
    hosted VST3) and adds two controls the chain itself does not have:

    - **Bypass**: an atomic flag. When set the wrapper returns without touching
      the buffer and reports zero latency, so bypassing a look-ahead effect
      immediately removes its delay from the chain total.
    - **Wet/dry**: an atomic 0..1 balance applied with a dry copy taken before
      the wrapped processor runs. `wet == 1` skips the blend and keeps the
      processed signal untouched; `wet == 0` is a bit-exact dry passthrough.

    Threading:

    - `prepareToPlay()`, `releaseResources()`, `reset()`, the parameter setters
      and the wrapped processor's own setters are message-thread calls.
    - `setBypassed()` / `setWet()` are lock-free atomic stores, so the API may
      change them between blocks without touching the audio thread contract.
    - `processBlock()` is the audio path: it only writes memory allocated in
      `prepareToPlay()`; no allocation, locks or logging.
*/
class InsertEffect final : public juce::AudioProcessor
{
public:
    InsertEffect (std::unique_ptr<juce::AudioProcessor> processorToWrap,
                  FxInsertType typeToUse,
                  std::string idToUse,
                  std::string nameToUse);
    ~InsertEffect() override = default;

    //==============================================================================
    // AudioProcessor

    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    void reset() override;

    using juce::AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    bool hasEditor() const override                 { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    const juce::String getName() const override     { return effectName; }
    bool acceptsMidi() const override               { return false; }
    bool producesMidi() const override              { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override                   { return 1; }
    int getCurrentProgram() override                { return 0; }
    void setCurrentProgram (int) override           {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Insert controls (lock-free)

    void setBypassed (bool shouldBeBypassed) noexcept;
    bool isBypassed() const noexcept;

    void setWet (float wetAmount) noexcept;
    float getWet() const noexcept;

    /** Re-reads the wrapped processor's latency, publishes it through
        `AudioProcessor::setLatencySamples()` (zero while bypassed or fully
        dry) and caches it. Off the audio thread. */
    void refreshLatency() noexcept;

    //==============================================================================
    juce::AudioProcessor* getWrappedProcessor() const noexcept { return processor.get(); }
    FxInsertType getType() const noexcept { return type; }
    const std::string& getId() const noexcept { return id; }

private:
    std::unique_ptr<juce::AudioProcessor> processor;
    const FxInsertType type;
    const std::string id;
    const juce::String effectName;

    std::atomic<bool> bypassed { false };
    std::atomic<float> wet { 1.0f };
    std::atomic<int> cachedLatency { 0 };

    juce::AudioBuffer<float> dry;
    int maximumBlockSize = 0;
    int numChannels = 2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InsertEffect)
};

} // namespace AudioEngine
