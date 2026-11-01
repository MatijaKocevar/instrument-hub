#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <cstdint>

namespace ihub::session
{

/**
    Lock-free MIDI hand-off from one producer (MIDI input thread) to the audio
    callback.

    Storage is fixed at construction, so `push()` and `drain()` never allocate,
    lock or log. SysEx is deliberately dropped: forwarding a multi-message
    stream in pieces would need unbounded storage.
*/
class MidiBridge
{
public:
    static constexpr int queueCapacity = 1024;

    MidiBridge() = default;

    /** Sizes the block buffer once, in `prepare()`. */
    void prepare();

    /** Drops events queued for a stopped stream. */
    void reset() noexcept;

    /** Queues one short MIDI message for the next block. */
    bool push (const juce::MidiMessage& message) noexcept;

    /** Audio-thread call: moves queued messages into `destination` at sample 0. */
    void drain (juce::MidiBuffer& destination) noexcept;

    int getQueuedCount() const noexcept { return fifo.getNumReady(); }

    juce::MidiBuffer& block() noexcept { return blockMidi; }

private:
    struct QueuedMidi
    {
        std::array<std::uint8_t, 3> data {};
        std::uint8_t size = 0;
    };

    juce::AbstractFifo fifo { queueCapacity };
    std::array<QueuedMidi, queueCapacity> queue {};
    juce::MidiBuffer blockMidi;
};

} // namespace ihub::session
