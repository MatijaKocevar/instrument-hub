#pragma once

#include "Bus.h"
#include "LatencyProbe.h"
#include "Slot.h"

#include <juce_audio_devices/juce_audio_devices.h>

#include <memory>
#include <vector>

namespace AudioEngine
{

/**
    The fixed audio graph.

    The engine owns a fixed set of `Slot` objects, all wired in parallel into a
    summing `Bus` that feeds a master limiter. Nothing is ever rewired: a slot
    is switched on or off by changing its state, and a 2-5 ms gain ramp hides
    the transition.

    Allocation rules:

    - `prepare()` allocates every buffer at the maximum block size, once.
    - `processBlock()` and the `AudioIODeviceCallback` path allocate nothing,
      take no locks and log nothing.

    `prepare()` and `reset()` must not run concurrently with `processBlock()`;
    `prepare()` is normally driven from `audioDeviceAboutToStart()`, or called
    explicitly by the application before it starts streaming.
*/
class AudioEngine final : public juce::AudioIODeviceCallback
{
public:
    static constexpr int defaultNumSlots = 8;
    static constexpr int defaultNumChannels = 2;

    explicit AudioEngine (int numSlots = defaultNumSlots,
                          int numChannels = defaultNumChannels);
    ~AudioEngine() override = default;

    //==============================================================================
    // Lifecycle (off the audio thread)

    /** Sizes every buffer and prepares all slots and their sources.

        May be called again after a device change. It never allocates after the
        first call with the same or smaller maximum block size.
    */
    void prepare (double sampleRate, int maximumBlockSize, int numChannels = defaultNumChannels);

    /** Releases the audio buffers. The stream must be stopped. */
    void releaseResources();

    bool isPrepared() const noexcept { return prepared; }

    //==============================================================================
    // Graph

    int getNumSlots() const noexcept { return static_cast<int> (slots.size()); }

    Slot& getSlot (int index) noexcept;
    const Slot& getSlot (int index) const noexcept;

    Bus& getBus() noexcept { return bus; }
    const Bus& getBus() const noexcept { return bus; }

    double getSampleRate() const noexcept { return sampleRate; }
    int getMaximumBlockSize() const noexcept { return maximumBlockSize; }
    int getNumChannels() const noexcept { return numChannels; }

    /** Largest latency reported by any slot's source, in samples.

        The bus does not delay short chains yet; this is the value a later
        delay-compensation stage has to align the others to.
    */
    int getLatencySamples() const noexcept;

    /** Measures the device's real round-trip latency with an impulse.

        Intended for the UI and command line: it briefly opens a full-duplex
        Loopback device, so it must not be called from the audio thread or
        while a set is streaming on that same device.
    */
    LatencyResult measureRoundTripLatency (const LatencyProbeOptions& options = {});

    //==============================================================================
    // Audio thread

    /** Renders one block into `output` and copies the mix there.

        `output.getNumSamples()` samples are rendered. Channels the engine does
        not produce are cleared. Real-time safe.
    */
    void processBlock (juce::AudioBuffer<float>& output, juce::MidiBuffer& midi) noexcept;

    /** Same as above, with an explicit block length (<= max block size). */
    void processBlock (juce::AudioBuffer<float>& output,
                       juce::MidiBuffer& midi,
                       int numSamples) noexcept;

    /** Clears the bus and returns every slot to idle. Stream must be stopped. */
    void reset() noexcept;

    //==============================================================================
    // juce::AudioIODeviceCallback

    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                           int numInputChannels,
                                           float* const* outputChannelData,
                                           int numOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override;

private:
    void renderBlock (int numSamples, juce::MidiBuffer& midi) noexcept;
    void copyToOutput (juce::AudioBuffer<float>& output, int numSamples) noexcept;

    std::vector<std::unique_ptr<Slot>> slots;
    Bus bus;

    juce::AudioBuffer<float> renderBuffer;
    juce::MidiBuffer deviceMidi;

    double sampleRate = 48000.0;
    int maximumBlockSize = 0;
    int numChannels = defaultNumChannels;
    bool prepared = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};

} // namespace AudioEngine
