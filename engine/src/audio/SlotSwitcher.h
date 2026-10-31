#pragma once

#include "Slot.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <atomic>
#include <memory>
#include <vector>

namespace AudioEngine
{

/**
    Armed-slot switching with an equal-power crossfade.

    The graph never rewires: every attached `Slot` stays where it is. Arming a
    slot only changes which slots are rendered and how their outputs are
    weighted:

    - `idle`    not rendered, gain 0
    - `warming` fading in, gain 0 -> 1
    - `active`  fully armed, gain 1
    - `fading`  fading out, gain 1 -> 0

    `armSlot()` is a control-thread call that publishes one atomic request.
    The audio thread consumes that request at the next block boundary and runs
    a 3 ms equal-power crossfade between the outgoing and incoming slots:

        outgoing gain = g0 * cos (p * pi / 2)
        incoming gain = g0 + (1 - g0) * sin (p * pi / 2)

    where `g0` is the slot's gain when the transition starts and `p` runs from
    0 to 1 over the fade. Starting from the current gain is what makes a
    re-arm in the middle of a fade continuous instead of a step.

    Threading:

    - `setSlots()`, `prepare()` and `reset()` are message-thread calls. Stop the
      stream (or call them between blocks) before using them.
    - `armSlot()`, `disarm()` and `getArmedSlot()` are lock-free and safe from
      any thread while the stream runs.
    - `process()` is an audio-thread call: no allocation, no locks, no logging.
    - State and gain getters read atomics published by the audio thread.

    `SlotSwitcher` drives its slots through `Slot::processUnfaded()`, so the
    crossfade is the only envelope. Do not also call `Slot::process()` on the
    same slots.
*/
class SlotSwitcher
{
public:
    enum class State
    {
        idle,
        warming,
        active,
        fading
    };

    /** Stable text for logs, tests and controller LED mapping. */
    static const char* toString (State state) noexcept;

    static constexpr float defaultFadeMilliseconds = 3.0f;

    /** `fadeMilliseconds` is normally 3 ms.

        A value of 0 disables the ramp and switches at the next block boundary.
        That exists for continuity tests to prove the detector catches a step;
        never use it in a running set.
    */
    explicit SlotSwitcher (float fadeMilliseconds = defaultFadeMilliseconds) noexcept;
    ~SlotSwitcher() = default;

    //==============================================================================
    // Setup (off the audio thread)

    /** Attaches the fixed set of slots. Each pointer must stay valid while it
        is attached. Entries are addressed by their index in this list.

        Calling this after `prepare()` re-prepares the new slots with the last
        known format. The stream must be stopped.
    */
    void setSlots (const std::vector<Slot*>& newSlots);

    /** Prepares every attached slot and sizes the crossfade mix buffer.

        May be called again after a device change. A pending `armSlot()` request
        is kept and fades in on the first block after the new format is ready.
    */
    void prepare (double sampleRate, int maximumBlockSize, int numChannels);

    /** Returns every slot to `idle`, clears gains and drops the request.

        The audio stream must be stopped.
    */
    void reset() noexcept;

    bool isPrepared() const noexcept { return prepared; }

    int getNumSlots() const noexcept { return numSlots; }
    Slot* getSlot (int slotIndex) const noexcept;

    //==============================================================================
    // Control (lock-free, safe while the stream runs)

    /** Requests that `slotIndex` becomes the armed slot.

        The audio thread starts an equal-power crossfade at its next block
        boundary. Re-arming the slot that is already the target is a no-op.
        Arming the slot that is currently fading out reverses the fade from its
        current gain. Negative indices mean `disarm()`; out-of-range indices are
        ignored.
    */
    void armSlot (int slotIndex) noexcept;

    /** Requests that every slot fade out. */
    void disarm() noexcept;

    /** The latest requested slot, or -1 when disarmed. Not a confirmation that
        the audio thread has consumed the request yet.
    */
    int getArmedSlot() const noexcept;

    /** The slot's published state, as of the last processed block. */
    State getSlotState (int slotIndex) const noexcept;

    /** The slot's published crossfade gain, 0 .. 1, as of the last block. */
    float getSlotGain (int slotIndex) const noexcept;

    //==============================================================================
    // Audio thread

    /** Advances the crossfade, renders the participating slots and sums them
        into the output buffer.

        Returns true when at least one slot rendered. The result is valid for
        the first `numSamples` frames of `getOutputBuffer()`.
    */
    bool process (int numSamples, juce::MidiBuffer& midi) noexcept;

    /** The crossfaded sum, sized in `prepare()` to the maximum block size. */
    const juce::AudioBuffer<float>& getOutputBuffer() const noexcept { return mixBuffer; }

private:
    struct RuntimeState
    {
        State state = State::idle;
        float gain = 0.0f;      // gain applied to the most recent sample
        float startGain = 0.0f; // gain when the current transition began
        float targetGain = 0.0f;
    };

    void beginTransition (int slotIndex) noexcept;
    float gainAt (const RuntimeState& state, float phase) const noexcept;
    void publishStates() noexcept;

    std::vector<Slot*> slots;
    std::vector<RuntimeState> runtime;

    std::unique_ptr<std::atomic<int>[]> publishedStates;
    std::unique_ptr<std::atomic<float>[]> publishedGains;

    juce::AudioBuffer<float> mixBuffer;

    double sampleRate = 0.0;
    int maximumBlockSize = 0;
    int numChannels = 0;
    int numSlots = 0;

    const float fadeMilliseconds;
    float fadeIncrement = 1.0f;
    bool prepared = false;

    // Control -> audio request. The audio thread owns everything else.
    std::atomic<int> pendingSlot { -1 };

    // Audio-thread-owned state.
    int consumedSlot = -1;
    int incomingSlot = -1;
    float fadePosition = 0.0f;
    bool crossfading = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SlotSwitcher)
};

} // namespace AudioEngine
