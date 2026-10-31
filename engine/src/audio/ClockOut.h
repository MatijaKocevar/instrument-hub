#pragma once

#include "Transport.h"

#include <atomic>
#include <cstdint>

namespace AudioEngine
{

/**
    A destination for the clock's one-byte system real-time messages.

    `ClockOut` never touches a MIDI device itself: the audio thread produces
    timestamped events into a lock-free queue and the control/message thread
    drains them into a sink. That keeps device I/O and any driver lock off the
    audio callback.

    To send on a chosen `juce::MidiOutput`, adapt it on the message thread:

        class JuceSink final : public AudioEngine::MidiSink
        {
        public:
            explicit JuceSink (juce::MidiOutput& deviceToUse) : device (deviceToUse) {}

            void sendRealTime (std::uint8_t statusByte) noexcept override
            {
                device.sendMessageNow (juce::MidiMessage (statusByte));
            }

        private:
            juce::MidiOutput& device;
        };

    then call `clockOut.drainTo (sink)` from a timer or the message loop.
*/
class MidiSink
{
public:
    virtual ~MidiSink() = default;

    /** Sends one MIDI system real-time status byte (0xF8, 0xFA, 0xFB or 0xFC). */
    virtual void sendRealTime (std::uint8_t statusByte) noexcept = 0;
};

//==============================================================================

/**
    24 PPQN MIDI clock generator locked to the engine `Transport`.

    The transport is the only clock source: positions are derived from PPQ and
    the device sample rate, never from a wall clock or a periodic callback. A
    tick at musical position `k / 24` is placed at the block sample offset that
    corresponds to that PPQ, so block size does not affect the emitted timing.
    At 120 BPM and 48 kHz a tick lands every exactly 1000 frames (20.833 ms).

    Messages:

    - `clock`    (0xF8) at every 1/24 quarter note while running.
    - `start`    (0xFA) when playback begins at position zero.
    - `continue` (0xFB) when playback begins at a non-zero position.
    - `stop`     (0xFC) when playback stops.

    Usage on the audio thread, before advancing the transport:

        clockOut.process (transport, numSamples);
        transport.process (numSamples);

    `process()` fills a fixed-size lock-free queue; nothing allocates, locks or
    logs. On the message thread, `drainTo()` sends the queued events in order.

    Tests and offline rendering can use `generate()` instead, which writes
    events straight into a caller-owned buffer and returns how many were
    written. Do not mix `generate()` and `process()` in one session: both
    advance the generator state.

    A rewind or reposition while playing is treated as a discontinuity: ticks
    covered by the jump are not emitted, and the next tick is resynchronised
    to the new position.
*/
class ClockOut final
{
public:
    /** Ticks per quarter note for MIDI system real-time clock. */
    static constexpr int pulsesPerQuarterNote = 24;

    /** The four messages the generator emits. The values are the MIDI bytes. */
    enum class Type : std::uint8_t
    {
        clock     = 0xF8,
        start     = 0xFA,
        continue_ = 0xFB,
        stop      = 0xFC
    };

    /** One timestamped message, in blocks of the transport that produced it. */
    struct Event
    {
        Type type = Type::clock;

        /** Frames from the start of the block; 0..numSamples-1. */
        int sampleOffset = 0;

        /** Absolute played frame since the last transport rewind.

            This is the transport-derived timestamp: tick `k` at a constant
            tempo is at frame `k * 60 * sampleRate / (bpm * 24)`.
        */
        std::int64_t transportSample = 0;

        /** Absolute 24-PPQN tick index for `clock`, -1 for transport messages. */
        std::int64_t tick = -1;

        /** Musical position of the event in quarter notes. */
        double ppq = 0.0;

        /** The MIDI system real-time byte to send. */
        std::uint8_t midiByte() const noexcept
        {
            return static_cast<std::uint8_t> (type);
        }
    };

    /** Largest number of events a single block can produce. */
    static constexpr int maxEventsPerBlock = 256;

    /** Capacity of the lock-free audio -> message thread queue. */
    static constexpr int queueCapacity = 2048;

    ClockOut() = default;
    ~ClockOut() = default;

    ClockOut (const ClockOut&) = delete;
    ClockOut& operator= (const ClockOut&) = delete;

    //==============================================================================
    // Control thread

    /** Forgets the running state, the last position and the tick counter.

        No start/stop/continue message is produced by the reset itself. Call it
        while the audio stream is stopped, e.g. when loading a new set.
    */
    void reset() noexcept;

    /** Discards queued events and returns how many were dropped. */
    int clearQueue() noexcept;

    //==============================================================================
    // Audio thread

    /** Produces this block's events into the lock-free queue.

        Call once per processed block, before `Transport::process()`. A
        transport that is not prepared (sample rate <= 0) produces nothing.
    */
    void process (const Transport& transport, int numSamples) noexcept;

    /** Produces this block's events into `destination`.

        Returns the number written, up to `capacity`. Events that do not fit are
        counted by `getDroppedEventCount()` and skipped; the tick counter still
        advances so later blocks stay in phase. This is the deterministic form
        used by tests and offline rendering.
    */
    int generate (const Transport& transport,
                  int numSamples,
                  Event* destination,
                  int capacity) noexcept;

    //==============================================================================
    // Message thread

    /** Pops up to `capacity` events, oldest first. Returns the number copied. */
    int drain (Event* destination, int capacity) noexcept;

    /** Sends every queued event to `sink`, oldest first. Returns the count. */
    int drainTo (MidiSink& sink) noexcept;

    /** Events waiting for the message thread. */
    int getPendingEventCount() const noexcept;

    /** Events lost because the queue or the caller's buffer was full. */
    std::int64_t getDroppedEventCount() const noexcept;

    //==============================================================================
    // Inspection (any thread)

    /** True when the transport was playing at the last process/generate call. */
    bool isRunning() const noexcept;

    /** Absolute tick index the generator will emit next. */
    std::int64_t getNextTick() const noexcept;

private:
    static std::int64_t firstTickAfter (double ppq) noexcept;

    bool push (const Event& event) noexcept;

    // Scheduler state (audio thread writes; the atomics are also readable
    // from inspection callers).
    std::atomic<bool> wasPlaying { false };
    double lastPpq = 0.0;
    std::atomic<std::int64_t> nextTick { 1 };

    // SPSC queue: audio thread writes, message thread reads.
    Event queue[queueCapacity] {};
    std::atomic<int> queueReadIndex { 0 };
    std::atomic<int> queueWriteIndex { 0 };
    std::atomic<std::int64_t> droppedEvents { 0 };
};

} // namespace AudioEngine
