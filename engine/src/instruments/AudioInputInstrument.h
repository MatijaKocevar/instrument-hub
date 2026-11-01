#pragma once

#include "Instrument.h"

#include "../audio/Slot.h"

#include <atomic>

namespace ihub
{

class AudioInputInstrument final : public Instrument,
                                   public AudioEngine::SlotSource
{
public:
    AudioInputInstrument() = default;
    ~AudioInputInstrument() override = default;

    void prepare (double sampleRate, int maxBlock) override;

    void setInputChannel (int channel) noexcept;
    int getInputChannel() const noexcept;

    void setGain (float linearGain) noexcept;
    float getGain() const noexcept;

    void setLatencySamples (int samples) noexcept;

    int latencySamples() const noexcept override;

    Kind kind() const noexcept override { return Kind::AudioInput; }

    void setDirectMonitor (bool shouldMonitor) noexcept;
    bool isDirectMonitor() const noexcept;

    void setLatencyBudgetSamples (int samples) noexcept;

    int latencyBudgetSamples() const noexcept;

    bool isWithinLatencyBudget() const noexcept;

    bool acceptsDeviceInput() const noexcept override { return true; }

    bool bypassesFxChain() const noexcept override;

    void setInputBlock (const float* const* channelData, int numChannels) noexcept override;

    void clearInput() noexcept;

    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    void noteOn (int ch, int note, float vel) override;

    void noteOff (int ch, int note) override;

private:
    static constexpr float maximumGain = 4.0f;

    double sampleRate = 48000.0;
    int maximumBlockSize = 0;

    const float* const* inputChannels = nullptr;
    int numInputChannels = 0;

    std::atomic<int> selectedInputChannel { 0 };
    std::atomic<float> gainTarget { 1.0f };
    std::atomic<int> cachedLatency { 0 };
    std::atomic<bool> directMonitor { false };
    std::atomic<int> latencyBudget { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioInputInstrument)
};

}
