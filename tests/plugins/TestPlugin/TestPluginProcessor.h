#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>

class TestPluginAudioProcessor final : public juce::AudioProcessor
{
public:
    static constexpr const char* gainParamID      = "gain";
    static constexpr const char* frequencyParamID = "frequency";
    static constexpr const char* latencyParamID   = "latencyMs";
    static constexpr const char* sineLevelParamID = "sineLevel";
    static constexpr const char* stateMarkerParamID = "stateMarker";
    static constexpr const char* midiGateParamID   = "midiGate";

    static constexpr float maxLatencyMs = 20.0f;

    TestPluginAudioProcessor();
    ~TestPluginAudioProcessor() override = default;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    using juce::AudioProcessor::processBlock;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "TestPlugin"; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    juce::AudioBuffer<float> delayBuffer;
    int delayWritePosition = 0;
    int maxDelaySamples = 0;
    int reportedLatency = 0;
    double phase = 0.0;
    double currentSampleRate = 44100.0;
    std::array<bool, 128> heldNotes {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TestPluginAudioProcessor)
};
