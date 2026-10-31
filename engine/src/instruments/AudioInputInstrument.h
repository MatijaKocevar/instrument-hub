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
    returns it. The instrument itself introduces no delay. That cached value is
    the input's own latency budget: `setLatencyBudgetSamples()` records the
    maximum the input path may use for monitoring, independently of any FX
    chain the slot hosts.

    Direct monitoring (`setDirectMonitor(true)`) is the bypass path: the slot
    skips its FX chain, so the performer hears the interface input at the
    input's own latency while a heavy chain stays loaded and prepared.

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
    // Direct monitoring

    /** Routes the input straight to the bus, skipping the slot's FX chain.

        In direct-monitor mode `process()` still renders the input exactly as
        it does normally; what changes is that the slot leaves the FX chain out
        of the audible path. The performer therefore hears the interface input
        with the input path's own latency, not the chain's. A heavy effect
        (amp sim, convolution reverb) can stay loaded and prepared without
        adding monitoring delay.

        The flag is lock-free and takes effect on the next block. Off the audio
        thread.
    */
    void setDirectMonitor (bool shouldMonitor) noexcept;
    bool isDirectMonitor() const noexcept;

    /** The maximum input-path latency allowed for monitoring, in samples.

        `latencySamples()` is compared against it; the FX chain is not part of
        the comparison because direct monitoring never includes it. A negative
        value clears the budget, which makes `isWithinLatencyBudget()` true.
        Off the audio thread.
    */
    void setLatencyBudgetSamples (int samples) noexcept;

    /** The configured budget, or -1 when no budget is set. */
    int latencyBudgetSamples() const noexcept;

    /** True when the cached input latency is inside the configured budget. */
    bool isWithinLatencyBudget() const noexcept;

    //==============================================================================
    // AudioEngine::SlotSource

    /** The audio input is the slot source that consumes the device's input. */
    bool acceptsDeviceInput() const noexcept override { return true; }

    /** Direct monitoring is exactly the bypass-the-chain request. */
    bool bypassesFxChain() const noexcept override;

    //==============================================================================
    // Audio thread

    /** Hands the current device block to the instrument.

        Called on the audio thread immediately before `process()`, normally by
        `AudioEngine` through the slot (`Slot::feedDeviceInput()`), not by hand.
        The pointer array and the samples it points at must stay valid until
        that `process()` returns. The hand-off is consumed by one block only.
    */
    void setInputBlock (const float* const* channelData, int numChannels) noexcept override;

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
    std::atomic<bool> directMonitor { false };
    std::atomic<int> latencyBudget { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioInputInstrument)
};

} // namespace ihub
