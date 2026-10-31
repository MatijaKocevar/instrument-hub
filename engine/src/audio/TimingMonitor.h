#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <vector>

namespace AudioEngine
{

/** One consistent-enough view of the callback timing counters.

    The fields are read from several atomics, so a snapshot taken while the
    audio thread is running can mix values from two adjacent callbacks. That is
    fine for meters and diagnostics; it is not a transactional view.
*/
struct TimingSnapshot
{
    std::uint64_t callbacks = 0;
    std::uint64_t overruns = 0;
    std::uint64_t droppedBlocks = 0;
    std::uint64_t samplesProcessed = 0;

    double sampleRate = 0.0;
    int blockSize = 0;

    /** Time available for one callback: `blockSize * 1000 / sampleRate`. */
    double deadlineMs = 0.0;

    double lastCallbackMs = 0.0;
    double rollingMaxCallbackMs = 0.0;
    double worstCallbackMs = 0.0;
    double averageCallbackMs = 0.0;
    double lastLoadPercent = 0.0;

    int windowSize = 0;
};

/**
    Measures how long each audio callback takes and compares it with the block
    deadline.

    An **overrun** (an xrun in the audio-device sense) is a callback whose
    measured duration is strictly greater than the deadline. A **dropped block**
    is a complete extra block period consumed by an overrun: a callback taking
    exactly 2x the deadline drops one block, one taking 3x drops two, and one
    taking 1.5x drops none. Both are inferred from the callback duration, which
    is the only thing a callback can observe about itself.

    The rolling maximum is the largest duration among the last `windowSize`
    callbacks. It is maintained with a monotonic deque in pre-allocated storage,
    so recording a callback is O(1) amortised, allocation-free, and lock-free.

    Threading:

    - `beginCallback()`, `endCallback()` and `recordCallback()` run on the audio
      thread.
    - The getters and `snapshot()` may be called from any thread.
    - `prepare()` and `setDeadlineMs()` are for setup / message-thread use.
    - `reset()` clears history and must only be called while the audio stream is
      stopped.

    `recordCallback()` takes an already-measured duration. It exists so tests
    can inject an exact stall without sleeping, and so callers that time a
    callback another way can feed the same accounting.
*/
class TimingMonitor final
{
public:
    static constexpr int defaultWindowSize = 512;

    /** Opaque start value returned by `beginCallback()`. */
    struct CallbackToken
    {
        std::chrono::steady_clock::time_point started;
    };

    explicit TimingMonitor (int windowSize = defaultWindowSize);
    ~TimingMonitor() = default;

    TimingMonitor (const TimingMonitor&) = delete;
    TimingMonitor& operator= (const TimingMonitor&) = delete;

    //==============================================================================
    // Setup (off the audio thread)

    /** Sets the sample rate, block size and the deadline derived from them. */
    void prepare (double sampleRate, int blockSize) noexcept;

    /** Overrides the deadline directly. Values <= 0 disable overrun counting. */
    void setDeadlineMs (double newDeadlineMs) noexcept;

    /** Clears every counter, peak and the rolling window.

        The audio stream must be stopped; the rolling window is owned by the
        audio thread.
    */
    void reset() noexcept;

    //==============================================================================
    // Audio thread

    /** Starts timing one callback. */
    CallbackToken beginCallback() const noexcept;

    /** Ends timing and records the result.

        `numSamplesProcessed` is added to the sample counter when positive.
    */
    void endCallback (CallbackToken token, int numSamplesProcessed = -1) noexcept;

    /** Records one callback that already has a measured duration in ms.

        Negative or non-finite durations are treated as zero. This is the
        deterministic injection seam used by tests.
    */
    void recordCallback (double elapsedMs, int numSamplesProcessed = -1) noexcept;

    //==============================================================================
    // Inspection (any thread)

    double getDeadlineMs() const noexcept;

    std::uint64_t getCallbackCount() const noexcept;
    std::uint64_t getOverrunCount() const noexcept;

    /** Alias for `getOverrunCount()`: an overrun is an xrun. */
    std::uint64_t getXrunCount() const noexcept { return getOverrunCount(); }

    std::uint64_t getDroppedBlockCount() const noexcept;
    std::uint64_t getSamplesProcessed() const noexcept;

    double getLastCallbackMs() const noexcept;
    double getRollingMaxCallbackMs() const noexcept;
    double getWorstCallbackMs() const noexcept;
    double getAverageCallbackMs() const noexcept;

    /** `lastCallbackMs / deadlineMs * 100`, or 0 before a deadline is known. */
    double getLastLoadPercent() const noexcept;

    int getWindowSize() const noexcept { return windowSize; }

    TimingSnapshot snapshot() const noexcept;

private:
    struct WindowEntry
    {
        std::uint64_t sequence = 0;
        double milliseconds = 0.0;
    };

    void pushWindow (double milliseconds) noexcept;

    // Fixed-size circular monotonic deque, allocated once in the constructor.
    const int windowSize;
    std::vector<WindowEntry> window;
    int windowHead = 0;
    int windowTail = 0;
    int windowCount = 0;
    std::uint64_t sequence = 0;

    std::atomic<double> sampleRate { 0.0 };
    std::atomic<int> blockSize { 0 };
    std::atomic<double> deadlineMs { 0.0 };

    std::atomic<std::uint64_t> callbacks { 0 };
    std::atomic<std::uint64_t> overruns { 0 };
    std::atomic<std::uint64_t> droppedBlocks { 0 };
    std::atomic<std::uint64_t> samplesProcessed { 0 };

    std::atomic<double> lastCallbackMs { 0.0 };
    std::atomic<double> rollingMaxCallbackMs { 0.0 };
    std::atomic<double> worstCallbackMs { 0.0 };
    std::atomic<double> totalCallbackMs { 0.0 };
};

} // namespace AudioEngine
