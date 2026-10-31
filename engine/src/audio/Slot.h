#pragma once

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

    A slot owns its render buffer (allocated in `prepare()` at the maximum block
    size) and a 2-5 ms gain ramp that hides activation and deactivation. Wiring
    never changes: the engine calls `process()` on every slot whose state is not
    `idle`, then sums the results on its bus.

    `activate()`, `deactivate()`, `setGain()`, `setSource()`, `prepare()` and
    `reset()` are message-thread calls and must not race with `process()`; stop
    the stream (or call them between blocks) before using them. `process()` and
    the state getters are safe from the audio thread.
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

    /** The most recent render, valid only after `process()` returned true. */
    const juce::AudioBuffer<float>& getOutputBuffer() const noexcept { return view; }

    //==============================================================================
    // Inspection (off the audio thread)

    SlotState getState() const noexcept;
    bool isProcessing() const noexcept;

    int getIndex() const noexcept { return index; }

    /** Latency reported by the current source, in samples. */
    int latencySamples() const noexcept;

private:
    void applyGains (int numSamples) noexcept;

    const int index;
    const float fadeMilliseconds;

    juce::AudioBuffer<float> storage;
    juce::AudioBuffer<float> view;
    SlotSource* source = nullptr;

    double sampleRate = 0.0;
    int maximumBlockSize = 0;
    int numChannels = 0;
    bool prepared = false;

    std::atomic<bool> activated { false };
    std::atomic<SlotState> publishedState { SlotState::idle };
    std::atomic<float> gainTarget { 1.0f };

    // Audio-thread-owned state.
    SlotState state = SlotState::idle;
    float fadeGain = 0.0f;
    float fadeStep = 0.0f;
    float appliedGain = 1.0f;
    float gainStep = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Slot)
};

} // namespace AudioEngine
