#pragma once

#include "Transport.h"

#include <atomic>
#include <cstdint>

namespace AudioEngine
{

/**
    MIDI clock follower: slave mode.

    `ClockIn` consumes the four system real-time messages a clock master sends
    and turns them into one `Transport::ExternalSync` update per audio block.
    It never touches a MIDI device itself: the MIDI thread pushes raw bytes
    with their arrival time, the audio thread drains them from a lock-free
    queue and applies the result to the transport. Nothing on the audio path
    allocates, locks or logs.

    Messages:

    - `clock`    (0xF8) one 24-PPQN tick. Tempo is measured from the interval
                 between consecutive ticks; position is the tick count.
    - `start`    (0xFA) playback from position zero.
    - `continue` (0xFB) resume from the current position.
    - `stop`     (0xFC) playback stops; the position is held.

    Tempo estimation:

    Each tick interval is converted to a candidate BPM. The estimate is the
    median of the last `tempoHistorySize` intervals, so a single late or
    duplicated tick does not move it, then a one-pole low-pass smooths the
    result once it is trusted. An interval outside the tempo range
    (`Transport::minBpm`..`Transport::maxBpm`) is treated as a discontinuity:
    the run restarts and the estimate is re-acquired from fresh ticks.

    Locking:

    The tempo is reported as locked once `lockTicks` consecutive ticks have
    arrived with `minTempoIntervals` usable intervals, i.e. after one beat.
    At 120 BPM that is 500 ms, comfortably inside the two-second acceptance
    window. The transport is allowed to roll before the lock is confirmed,
    using the current estimate, exactly like a hardware slave that starts on
    `start`/`continue` and only reports sync once it has measured the master.

    Stop detection:

    A `stop` message stops playback immediately. Losing the clock without a
    message is caught as well: if no tick arrives for
    `max (0.5 s, six expected tick periods)`, the clock is reported lost and
    the transport stops. `isLocked()` is separate from clock presence: a
    stopped master that keeps sending clock stays locked but is not playing.

    Position:

    The position is `tickCount / 24` quarter notes, advanced by the block's
    sample count between ticks at the estimated tempo. A tick resets the
    phase at the block boundary it is drained in, so the worst-case phase
    error is one block; at the usual 64–1024 frame blocks that is under
    2.5–21 ms and never accumulates, because every tick re-anchors it.
    `process()` publishes the position at the start of its block, so calling
    it before `Transport::process()` keeps the two in step.

    A `stop` freezes the position, including the sub-tick phase, so playback
    resumes exactly where it stopped. Losing the clock freezes it at the last
    confirmed tick (the position cannot advance past the last tick it saw).
    A `start` rewinds to zero; a `continue` keeps the frozen position.

    Usage in the audio callback (after `prepare()`, before advancing the
    transport):

        clockIn.process (transport, numSamples);
        transport.process (numSamples);

    The caller must select `Transport::SyncSource::external`; while the
    source is internal `applyExternalSync()` ignores the updates and the
    transport runs on its own tempo. The message thread feeds the follower:

        clockIn.pushRealtime (statusByte, secondsSinceMonotonicOrigin);

    Tests and offline rendering push frame-derived timestamps, which gives
    the same result as a clock source that is sample-locked to the engine.
*/
class ClockIn final
{
public:
    /** Ticks per quarter note for MIDI system real-time clock. */
    static constexpr int pulsesPerQuarterNote = 24;

    /** Capacity of the MIDI -> audio thread event queue.

        256 events is more than five seconds of clock at 120 BPM, so a healthy
        audio callback never sees an overflow; drops are counted.
    */
    static constexpr int queueCapacity = 256;

    /** Tick intervals kept for the median tempo estimate. */
    static constexpr int tempoHistorySize = 32;

    /** Intervals needed before a tempo is trusted at all. */
    static constexpr int minTempoIntervals = 8;

    /** Consecutive ticks needed before the estimate is reported as locked. */
    static constexpr int lockTicks = 24;

    /** One-pole coefficient applied to the median after the first lock. */
    static constexpr double tempoSmoothing = 0.05;

    /** The four messages the follower understands. Values are the MIDI bytes. */
    enum class Type : std::uint8_t
    {
        clock     = 0xF8,
        start     = 0xFA,
        continue_ = 0xFB,
        stop      = 0xFC
    };

    /** A cheap, mostly consistent view of the follower for UI and API. */
    struct Snapshot
    {
        bool locked = false;
        bool playing = false;
        bool clockPresent = false;

        /** Smoothed estimate in BPM, 0 before any interval was measured. */
        double bpm = 0.0;

        /** Last published position in quarter notes. */
        double ppq = 0.0;

        /** Ticks received since the last `start`. */
        std::int64_t tickCount = 0;

        /** Consecutive ticks in the current run, used for the lock. */
        std::int64_t clockCount = 0;

        /** Seconds since the last real-time message. */
        double secondsSinceClock = 0.0;
    };

    ClockIn() = default;
    ~ClockIn() = default;

    ClockIn (const ClockIn&) = delete;
    ClockIn& operator= (const ClockIn&) = delete;

    //==============================================================================
    // Control thread

    /** Forgets the clock, the tempo estimate and the position.

        No transport update is produced by the reset itself; the next real-time
        message re-arms the follower. Call it while the stream is stopped or
        before switching the sync source.
    */
    void reset() noexcept;

    /** Discards queued events and returns how many were dropped. */
    int clearQueue() noexcept;

    //==============================================================================
    // MIDI thread

    /** Queues one system real-time byte with its monotonic arrival time.

        The timestamp only needs a stable origin (seconds from any monotonic
        clock); differences are what matter. Bytes outside the four real-time
        messages are ignored and return false. A full queue also returns false
        and is counted by `getDroppedEventCount()`.
    */
    bool pushRealtime (std::uint8_t statusByte, double timestampSeconds) noexcept;

    //==============================================================================
    // Audio thread

    /** Drains queued messages, updates the estimate and applies one transport
        update.

        Call once per processed block, before `Transport::process()`. A
        transport without a valid sample rate is left untouched.
    */
    void process (Transport& transport, int numSamples) noexcept;

    //==============================================================================
    // Inspection (any thread)

    /** True when the tempo estimate is trusted and the clock is present. */
    bool isLocked() const noexcept;

    /** True when a `start`/`continue` was received and the clock is present. */
    bool isPlaying() const noexcept;

    /** True while real-time messages keep arriving within the loss timeout. */
    bool isClockPresent() const noexcept;

    /** Smoothed tempo estimate in BPM, or 0 before the first interval. */
    double getBpm() const noexcept;

    /** Last published position in quarter notes. */
    double getPpq() const noexcept;

    /** Ticks counted for position since the last `start`. */
    std::int64_t getTickCount() const noexcept;

    /** Consecutive ticks in the current run, used for the lock. */
    std::int64_t getClockCount() const noexcept;

    /** Events waiting for the audio thread. */
    int getPendingEventCount() const noexcept;

    /** Events lost because the queue was full. */
    std::int64_t getDroppedEventCount() const noexcept;

    Snapshot snapshot() const noexcept;

private:
    struct Event
    {
        Type type = Type::clock;
        double timestampSeconds = 0.0;
    };

    bool push (const Event& event) noexcept;
    int drain (Event* destination, int capacity) noexcept;

    void handleEvent (const Event& event) noexcept;
    void resetTempoRun() noexcept;
    void addInterval (double seconds) noexcept;
    double medianInterval() const noexcept;

    /** Called when the clock is considered lost: stops and drops the lock. */
    void loseClock() noexcept;

    static double clampBpm (double bpm) noexcept;
    static double tickPeriodSeconds (double bpm) noexcept;

    // SPSC queue: MIDI thread writes, audio thread reads.
    Event queue[queueCapacity] {};
    std::atomic<int> queueReadIndex { 0 };
    std::atomic<int> queueWriteIndex { 0 };
    std::atomic<std::int64_t> droppedEvents { 0 };

    // Published state (audio thread writes, any thread reads).
    std::atomic<bool> locked { false };
    std::atomic<bool> playing { false };
    std::atomic<bool> clockPresent { false };
    std::atomic<double> bpm { 0.0 };
    std::atomic<double> ppq { 0.0 };
    std::atomic<std::int64_t> tickCount { 0 };
    std::atomic<std::int64_t> clockCount { 0 };
    std::atomic<double> secondsSinceClock { 0.0 };

    // Audio-thread state. Not read from anywhere else.
    double estimateBpm = 0.0;
    double intervalHistory[tempoHistorySize] {};
    int intervalCount = 0;
    int intervalHead = 0;

    bool haveTimestamp = false;
    double lastTimestamp = 0.0;

    bool seenClock = false;
    bool playingInternal = false;
    bool lockedInternal = false;

    std::int64_t positionTicks = 0;
    std::int64_t clockCountInternal = 0;
    double samplesSinceTick = 0.0;
    double sinceClockSeconds = 0.0;
};

} // namespace AudioEngine
