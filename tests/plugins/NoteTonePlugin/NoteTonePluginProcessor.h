#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>

class NoteTonePluginProcessor final : public juce::AudioProcessor
{
public:
    static constexpr float toneFrequency = 440.0f;
    static constexpr float toneAmplitude = 0.25f;

    NoteTonePluginProcessor();
    ~NoteTonePluginProcessor() override = default;

    using juce::AudioProcessor::processBlock;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>& buffer,
                       juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }

    const juce::String getName() const override { return "NoteTonePlugin"; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

private:
    double currentSampleRate = 44100.0;
    double phase = 0.0;
    std::array<bool, 128> heldNotes {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoteTonePluginProcessor)
};
