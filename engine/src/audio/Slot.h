#pragma once

#include "FxChain.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <atomic>

namespace AudioEngine
{

/** The processing seam between a Slot and whatever produces sound for it.

    This is deliberately narrower than the `Instrument` interface in
    `plan/DECISIONS.md`: the fixed graph only needs "prepare, process, report
    latency". T030's `Instrument` plugs in here through a thin adapter, so the
    graph does not grow a dependency on the instrument layer.

    `process()` is called on the audio thread and must obey the audio-thread
    contract: no allocation, no locks, no logging, no file I/O.
*/
class SlotSource
{
public:
    virtual ~SlotSource() = default;

    /** Called off the audio thread, before the first `process()` call. */
    virtual void prepare (double sampleRate, int maximumBlockSize) = 0;

    /** Renders `buffer.getNumSamples()` samples of this source.

        `buffer` has exactly the engine's channel count. `midi` is the block's
        MIDI, shared with every processing slot; a source must not modify it.
    */
    virtual void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) = 0;

    /** Processing delay this source introduces, in samples. */
    virtual int latencySamples() const noexcept { return 0; }

    //==============================================================================
    // Device input

    /** True when this source consumes the interface's input channels.

        The engine hands such a source the current device input block once per
        callback, before `process()`. Audio-thread call.
    */
    virtual bool acceptsDeviceInput() const noexcept { return false; }

    /** Receives the current device input block.

        Only called when `acceptsDeviceInput()` is true. The pointer array and
        the samples it points at are valid until the next `process()` returns;
        a source must not keep them across callbacks. Audio-thread call.
    */
    virtual void setInputBlock (const float* const* channelData,
                                int numChannels) noexcept
    {
        juce::ignoreUnused (channelData, numChannels);
    }

    /** True when this source must reach the bus without the slot's FX chain.

        Direct monitoring asks the slot to skip the chain entirely, so the
        interface input keeps its own latency instead of the chain's and a
        heavy effect cannot delay what the performer hears. Asked once per
        rendered block and once per latency query; must be audio-thread safe.
    */
    virtual bool bypassesFxChain() const noexcept { return false; }
};

/** The four states a slot can be in.

    Only `fadingIn`, `active` and `fadingOut` cause the source to be processed;
    `idle` slots are skipped entirely.
*/
enum class SlotState
{
    idle,
    fadingIn,
    active,
    fadingOut
};

/** Stable text for logs and tests. */
const char* toString (SlotState state) noexcept;

/**
    One fixed slot in the audio graph.

    A slot is the path instrument -> ordered `FxChain` -> bus. It owns its
    render buffer (allocated in `prepare()` at the maximum block size) and a
    2-5 ms gain ramp that hides activation and deactivation. Wiring never
    changes: the engine calls `process()` on every slot whose state is not
    `idle`, then sums the results on its bus.

    The FX chain holds non-owning `juce::AudioProcessor*` entries, so built-in
    and hosted effects use the same path. An empty chain is a bit-exact
    passthrough.

    `activate()`, `deactivate()`, `setGain()`, `setSource()`, `prepare()` and
    `reset()` are message-thread calls and must not race with `process()`; stop
    the stream (or call them between blocks) before using them. Editing the
    chain through `getFxChain()` follows the same rule. `process()` and the
    state getters are safe from the audio thread.
*/
class Slot
{
public:
    static constexpr float defaultFadeMilliseconds = 3.0f;

    explicit Slot (int index, float fadeMilliseconds = defaultFadeMilliseconds);
    ~Slot() = default;

    //==============================================================================
    // Setup (off the audio thread)

    /** Allocates the render buffer and prepares the current source.

        Safe to call again when the device changes; it never allocates on the
        audio thread.
    */
    void prepare (double sampleRate, int maximumBlockSize, int numChannels);

    /** Returns the slot to `idle` and clears its activation request.

        The audio stream must be stopped.
    */
    void reset() noexcept;

    /** Sets the source processed by this slot. Passing nullptr makes the slot
        silent. The audio stream must be stopped while this is called.
    */
    void setSource (SlotSource* newSource) noexcept;
    SlotSource* getSource() const noexcept { return source; }

    /** The effect chain between the source and the bus.

        Non-owning entries: the caller keeps each `juce::AudioProcessor` alive
        while it is in the chain. Edit it only with the stream stopped.
    */
    FxChain& getFxChain() noexcept { return fxChain; }
    const FxChain& getFxChain() const noexcept { return fxChain; }

    /** Gates the FX chain while the control thread rebuilds it.

        The audio thread reads this once per rendered block. While it is set the
        slot renders its source dry (the chain is skipped), which gives the
        message/HTTP thread a block in which the chain vector may be edited
        without racing the audio callback. Editing is rare, so the occasional
        dry block is acceptable and keeps the audio path lock-free.

        Lock-free; safe from any thread.
    */
    void setFxChainEditing (bool editing) noexcept
    {
        fxChainEditing.store (editing, std::memory_order_release);
    }

    bool isFxChainEditing() const noexcept
    {
        return fxChainEditing.load (std::memory_order_acquire);
    }

    //==============================================================================
    // Control (off the audio thread, lock-free)

    /** Requests that the slot fade in and start processing. */
    void activate() noexcept;

    /** Requests that the slot fade out and stop processing. */
    void deactivate() noexcept;

    /** The latest activation request, whether or not the ramp has finished. */
    bool isActivated() const noexcept;

    /** Linear gain applied on top of the fade, 0 .. 4. */
    void setGain (float newGain) noexcept;
    float getGain() const noexcept;

    //==============================================================================
    // Audio thread

    /** Processes one block if the slot is fading in, active or fading out.

        Renders into this slot's buffer and applies the fade/gain ramp.
        Returns true when the slot produced audio this block; the caller then
        adds `getOutputBuffer()` to the mix.
    */
    bool process (int numSamples, juce::MidiBuffer& midi) noexcept;

    /** Hands the current device input block to the source.

        Call once per callback, before `process()`. Audio thread and
        real-time safe. Sources that do not accept device input ignore it.
    */
    void feedDeviceInput (const float* const* channelData, int numChannels) noexcept;

    /** Renders the source and FX chain without this slot's activation fade.

        `SlotSwitcher` owns the crossfade envelope for the slots it arms, so
        running the slot's own 2-5 ms fade as well would stack two envelopes
        and break the equal-power law. This path applies the user gain ramp but
        leaves the slot's `idle`/`fadingIn`/`active`/`fadingOut` state machine
        and its published state untouched.

        Audio thread, real-time safe. A slot driven by `SlotSwitcher` must not
        also be driven through `process()`.
    */
    bool processUnfaded (int numSamples, juce::MidiBuffer& midi) noexcept;

    /** The most recent render, valid only after `process()` or
        `processUnfaded()` returned true.
    */
    const juce::AudioBuffer<float>& getOutputBuffer() const noexcept { return view; }

    //==============================================================================
    // Inspection (off the audio thread)

    SlotState getState() const noexcept;
    bool isProcessing() const noexcept;

    int getIndex() const noexcept { return index; }

    /** Latency reported by the current source plus the FX chain, in samples.

        When the source bypasses the chain (direct monitoring), the chain is
        not in the audible path, so only the source latency is reported.
    */
    int latencySamples() const noexcept;

    /** The same total expressed in milliseconds at the prepared sample rate.

        Returns 0 before the slot has been prepared, so a control-thread reader
        never divides by a zero sample rate.
    */
    double latencyMilliseconds() const noexcept;

private:
    bool renderSource (int numSamples, juce::MidiBuffer& midi) noexcept;
    void applyGains (int numSamples, bool applyFade) noexcept;

    const int index;
    const float fadeMilliseconds;

    juce::AudioBuffer<float> storage;
    juce::AudioBuffer<float> view;
    SlotSource* source = nullptr;
    FxChain fxChain;

    double sampleRate = 0.0;
    int maximumBlockSize = 0;
    int numChannels = 0;
    bool prepared = false;

    std::atomic<bool> activated { false };
    std::atomic<SlotState> publishedState { SlotState::idle };
    std::atomic<float> gainTarget { 1.0f };
    std::atomic<bool> fxChainEditing { false };

    // Audio-thread-owned state.
    SlotState state = SlotState::idle;
    float fadeGain = 0.0f;
    float fadeStep = 0.0f;
    float appliedGain = 1.0f;
    float gainStep = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Slot)
};

} // namespace AudioEngine
