#pragma once

#include "FxInsertTypes.h"

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <atomic>
#include <memory>
#include <string>

namespace AudioEngine
{

class InsertEffect final : public juce::AudioProcessor
{
public:
    InsertEffect (std::unique_ptr<juce::AudioProcessor> processorToWrap,
                  FxInsertType typeToUse,
                  std::string idToUse,
                  std::string nameToUse);
    ~InsertEffect() override = default;

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

    void setBypassed (bool shouldBeBypassed) noexcept;
    bool isBypassed() const noexcept;

    void setWet (float wetAmount) noexcept;
    float getWet() const noexcept;

    void refreshLatency() noexcept;

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

}
