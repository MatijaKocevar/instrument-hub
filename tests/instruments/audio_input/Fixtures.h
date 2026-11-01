#pragma once

#include "TestSupport.h"

#include "instruments/AudioInputInstrument.h"

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <atomic>

namespace AudioInputTest
{

using ihub::AudioInputInstrument;

constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 128;
constexpr unsigned int kChannels = 1;
constexpr unsigned int kPeriodFrames = 128;
constexpr unsigned int kBufferFrames = 16384;
constexpr unsigned int kTotalFrames = 48000;
constexpr double kFrequency = 440.0;
constexpr double kAmplitude = 0.5;
constexpr int kFadeBlocks = 8;

class GainFxProcessor final : public juce::AudioProcessor
{
public:
    explicit GainFxProcessor (float gainToUse = 1.0f, int latencyToReport = 0)
        : AudioProcessor (BusesProperties()
                              .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                              .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
          gain (gainToUse),
          latency (latencyToReport)
    {
    }

    const juce::String getName() const override { return "GainFxProcessor"; }

    void prepareToPlay (double, int) override
    {
        setLatencySamples (latency);
    }

    void releaseResources() override {}

    using juce::AudioProcessor::processBlock;

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        processCalls.fetch_add (1, std::memory_order_relaxed);

        if (std::abs (gain - 1.0f) > 0.0f)
            buffer.applyGain (gain);
    }

    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

    int getProcessCalls() const noexcept
    {
        return processCalls.load (std::memory_order_relaxed);
    }

private:
    float gain = 1.0f;
    int latency = 0;
    std::atomic<int> processCalls { 0 };
};

}
