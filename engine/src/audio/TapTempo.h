#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>

namespace AudioEngine
{

/**
    Tap-tempo estimator: the median of the last N tap intervals.

    A tap is a monotonic timestamp in seconds. The first tap of a sequence
    produces no estimate; every following tap produces one from the intervals
    between consecutive taps. Returning a median instead of a mean means one
    late or early tap does not drag the tempo with it, which is what a human
    tapping along expects.

    Sequences:

    - An interval shorter than `60 / maxBpm` or longer than `60 / minBpm`
      means the player stopped and started again; the tap becomes the first of
      a new sequence and the estimate returns to 0.
    - A non-increasing timestamp (the same or an earlier instant) does the
      same, so a double-fire or a clock that went backwards cannot produce a
      zero or negative interval.
    - A non-finite timestamp is ignored entirely.

    `historySize` is the number of *intervals* the median keeps (N), not the
    number of taps. Eight intervals is the default: at 120 BPM that is about
    four seconds of tapping memory.

    Threading:

    - `tapAt()`, `tap()` and `reset()` may be called from any control thread:
      a controller button callback runs on JUCE's MIDI thread while an API
      request runs on an HTTP thread, and both may tap the same estimator.
      The tap history is guarded by a short spin lock; only the occasional tap
      takes it, so there is no contention worth a mutex.
    - The getters are lock-free and safe from any thread.
    - This class must never be used from the audio thread. It is not part of
      the callback; the result is read by the control layer and published to
      the transport or the API.

    The estimator itself allocates nothing: the interval history is a fixed
    array sized for `maximumHistorySize`, so it is deterministic and safe to
    embed in a control object.
*/
class TapTempo final
{
public:
    /** Intervals kept by the default median window. */
    static constexpr int defaultHistorySize = 8;

    /** Upper bound for a custom history size. Storage is fixed at this size. */
    static constexpr int maximumHistorySize = 32;

    /** Tempo range a single interval may represent. */
    static constexpr double minBpm = 20.0;
    static constexpr double maxBpm = 300.0;

    /** Default tempo used by callers that have no estimate yet. */
    static constexpr double defaultBpm = 120.0;

    /** Intervals shorter than this start a new sequence (`maxBpm`). */
    static constexpr double minimumIntervalSeconds = 60.0 / maxBpm;

    /** Intervals longer than this start a new sequence (`minBpm`). */
    static constexpr double maximumIntervalSeconds = 60.0 / minBpm;

    /** `historySize` is clamped to 1..`maximumHistorySize`. */
    explicit TapTempo (int historySize = defaultHistorySize) noexcept;
    ~TapTempo() = default;

    TapTempo (const TapTempo&) = delete;
    TapTempo& operator= (const TapTempo&) = delete;

    //==============================================================================
    // Tapping (control threads)

    /** Records a tap at a monotonic timestamp in seconds.

        Returns the new estimate in BPM, or 0 when this tap started a new
        sequence (including the very first tap). A non-finite timestamp leaves
        the state untouched and returns the current estimate.
    */
    double tapAt (double timestampSeconds) noexcept;

    /** `tapAt()` using a monotonic wall clock. */
    double tap() noexcept;

    /** Clears the history and the estimate; the next tap starts over. */
    void reset() noexcept;

    //==============================================================================
    // Inspection (any thread)

    /** Current estimate in BPM, or 0 before a sequence has two taps. */
    double getBpm() const noexcept;

    /** True once `getBpm()` is non-zero. */
    bool hasEstimate() const noexcept;

    /** Taps recorded in the current sequence, including the first. */
    int getTapCount() const noexcept;

    /** Intervals currently in the median window: 0..`getHistorySize()`. */
    int getIntervalCount() const noexcept;

    /** The configured N. */
    int getHistorySize() const noexcept;

    /** Last accepted interval in seconds, or 0 after a reset. */
    double getLastIntervalSeconds() const noexcept;

    //==============================================================================
    // Helpers (any thread)

    /** Seconds in one beat at `tempo`. Non-positive tempos return 0. */
    static constexpr double intervalSecondsForBpm (double tempo) noexcept
    {
        return tempo > 0.0 ? 60.0 / tempo : 0.0;
    }

    /** Beats per minute for an interval in seconds. Non-positive input is 0. */
    static constexpr double bpmForIntervalSeconds (double seconds) noexcept
    {
        return seconds > 0.0 ? 60.0 / seconds : 0.0;
    }

private:
    /** Short critical section for the tap history. Never used on audio. */
    class ScopedLock
    {
    public:
        explicit ScopedLock (std::atomic_flag& target) noexcept
            : flag (target)
        {
            while (flag.test_and_set (std::memory_order_acquire))
            {
            }
        }

        ~ScopedLock()
        {
            flag.clear (std::memory_order_release);
        }

        ScopedLock (const ScopedLock&) = delete;
        ScopedLock& operator= (const ScopedLock&) = delete;

    private:
        std::atomic_flag& flag;
    };

    /** Median interval currently in the window; called with the lock held. */
    double medianIntervalSeconds() const noexcept;

    const int historySize;

    // The tap history below is guarded by `busy`. Plain members keep the
    // struct small and the critical section free of extra atomics.
    std::atomic_flag busy = ATOMIC_FLAG_INIT;

    int taps = 0;
    double lastTapTime = 0.0;
    double intervals[maximumHistorySize] {};
    int intervalCount = 0;
    int intervalHead = 0;

    // Published for lock-free inspection.
    std::atomic<double> bpm { 0.0 };
    std::atomic<int> tapCount { 0 };
    std::atomic<int> publishedIntervalCount { 0 };
    std::atomic<double> lastIntervalSeconds { 0.0 };
};

} // namespace AudioEngine
