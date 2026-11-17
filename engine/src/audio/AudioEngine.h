#pragma once

#include "Bus.h"
#include "LatencyProbe.h"
#include "Slot.h"
#include "TimingMonitor.h"

#include <juce_audio_devices/juce_audio_devices.h>

#include <memory>
#include <vector>

namespace AudioEngine {

class AudioEngine final : public juce::AudioIODeviceCallback {
public:
    static constexpr int defaultNumSlots = 8;
    static constexpr int defaultNumChannels = 2;

    explicit AudioEngine(int numSlots = defaultNumSlots, int numChannels = defaultNumChannels);
    ~AudioEngine() override = default;

    void prepare(double sampleRate, int maximumBlockSize, int numChannels = defaultNumChannels);

    void releaseResources();

    bool isPrepared() const noexcept { return prepared; }

    int getNumSlots() const noexcept { return static_cast<int>(slots.size()); }

    Slot& getSlot(int index) noexcept;
    const Slot& getSlot(int index) const noexcept;

    Bus& getBus() noexcept { return bus; }
    const Bus& getBus() const noexcept { return bus; }

    double getSampleRate() const noexcept { return sampleRate; }
    int getMaximumBlockSize() const noexcept { return maximumBlockSize; }
    int getNumChannels() const noexcept { return numChannels; }

    int getLatencySamples() const noexcept;

    LatencyResult measureRoundTripLatency(const LatencyProbeOptions& options = {});

    TimingMonitor& getTimingMonitor() noexcept { return timingMonitor; }
    const TimingMonitor& getTimingMonitor() const noexcept { return timingMonitor; }

    void processBlock(juce::AudioBuffer<float>& output, juce::MidiBuffer& midi) noexcept;

    void processBlock(juce::AudioBuffer<float>& output, juce::MidiBuffer& midi,
                      int numSamples) noexcept;

    void reset() noexcept;

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    void audioDeviceIOCallbackWithContext(
        const float* const* inputChannelData, int numInputChannels, float* const* outputChannelData,
        int numOutputChannels, int numSamples,
        const juce::AudioIODeviceCallbackContext& context) override;

private:
    void feedDeviceInput(const float* const* channelData, int numChannels) noexcept;
    void renderBlock(int numSamples, juce::MidiBuffer& midi) noexcept;
    void copyToOutput(juce::AudioBuffer<float>& output, int numSamples) noexcept;

    std::vector<std::unique_ptr<Slot>> slots;
    Bus bus;
    TimingMonitor timingMonitor;

    juce::AudioBuffer<float> renderBuffer;
    juce::MidiBuffer deviceMidi;

    double sampleRate = 48000.0;
    int maximumBlockSize = 0;
    int numChannels = defaultNumChannels;
    bool prepared = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};

}  // namespace AudioEngine
