#pragma once

#include "Instrument.h"

#include "../audio/Slot.h"

#include <atomic>

namespace ihub
{

/**
    The audio input as an `Instrument`: one interface channel, an optional
    gain stage, then out.

    The device callback owns the input samples. For every block it calls
    `setInputBlock()` with the device's channel pointers immediately before
    `process()`; `process()` consumes that block once and then forgets the
    pointers, so no device buffer is ever held across callbacks.

    `setInputChannel()` selects the 0-based interface channel copied out. The
    selected channel is written to every output channel, which makes a mono
    input audible on a stereo master bus without a separate pan stage.

    Latency is cached here rather than queried from the device per block: the
    owner calls `setLatencySamples()` with the interface's reported input
    latency (plus anything else the direct path adds), and `latencySamples()`
    returns it. The instrument itself introduces no delay.

    `process()` obeys the audio-thread contract: no allocation, no locks, no
    logging, no file I/O. Implements both `ihub::Instrument` and
    `AudioEngine::SlotSource`, so it plugs into today's fixed graph unchanged.
*/
class AudioInputInstrument final : public Instrument,
                                   public AudioEngine::SlotSource
{
public:
    AudioInputInstrument() = default;
    ~AudioInputInstrument() override = default;

    //==============================================================================
    // Setup (off the audio thread)

    /** Caches the sample rate and block size. The input block itself is not
        copied or allocated. */
    void prepare (double sampleRate, int maxBlock) override;

    /** Selects the 0-based interface channel copied to the output. */
    void setInputChannel (int channel) noexcept;
    int getInputChannel() const noexcept;

    /** Linear gain applied to the input, 0 .. 4. Default is unity. */
    void setGain (float linearGain) noexcept;
    float getGain() const noexcept;

    /** Sets the latency of the whole input path, in samples. */
    void setLatencySamples (int samples) noexcept;

    /** Latency cached by `setLatencySamples()`, in samples. */
    int latencySamples() const noexcept override;

    Kind kind() const noexcept override { return Kind::AudioInput; }

    //==============================================================================
    // Audio thread

    /** Hands the current device block to the instrument.

        Called on the audio thread immediately before `process()`. The pointer
        array and the samples it points at must stay valid until that
        `process()` returns. The hand-off is consumed by one block only.
    */
    void setInputBlock (const float* const* channelData, int numChannels) noexcept;

    /** Forgets the current block; the next `process()` renders silence. */
    void clearInput() noexcept;

    /** Renders the selected input channel into `buffer` with the gain applied.

        When no input block was supplied, or the selected channel is out of
        range, `buffer` is cleared. Audio thread only.
    */
    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    /** Audio input ignores note events. */
    void noteOn (int ch, int note, float vel) override;

    /** Audio input ignores note events. */
    void noteOff (int ch, int note) override;

private:
    static constexpr float maximumGain = 4.0f;

    double sampleRate = 48000.0;
    int maximumBlockSize = 0;

    // Audio-thread hand-off, written by setInputBlock() and consumed by the
    // next process() call on the same thread.
    const float* const* inputChannels = nullptr;
    int numInputChannels = 0;

    std::atomic<int> selectedInputChannel { 0 };
    std::atomic<float> gainTarget { 1.0f };
    std::atomic<int> cachedLatency { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioInputInstrument)
};

} // namespace ihub
