#include "TimingMonitor.h"

#include <algorithm>
#include <cmath>

namespace AudioEngine
{

namespace
{
constexpr double noDeadline = 0.0;

/** Tolerance used only to keep `2 * deadline / deadline` from rounding up to
    the next whole period at the boundary. */
constexpr double periodEpsilon = 1.0e-9;
} // namespace

TimingMonitor::TimingMonitor (int windowSizeToUse)
    : windowSize (std::max (1, windowSizeToUse)),
      window (static_cast<size_t> (std::max (1, windowSizeToUse)))
{
}

void TimingMonitor::prepare (double newSampleRate, int newBlockSize) noexcept
{
    if (! (newSampleRate > 0.0) || newBlockSize <= 0)
    {
        sampleRate.store (0.0, std::memory_order_relaxed);
        blockSize.store (0, std::memory_order_relaxed);
        deadlineMs.store (noDeadline, std::memory_order_relaxed);
        return;
    }

    sampleRate.store (newSampleRate, std::memory_order_relaxed);
    blockSize.store (newBlockSize, std::memory_order_relaxed);
    deadlineMs.store (static_cast<double> (newBlockSize) * 1000.0 / newSampleRate,
                      std::memory_order_relaxed);
}

void TimingMonitor::setDeadlineMs (double newDeadlineMs) noexcept
{
    deadlineMs.store (newDeadlineMs > 0.0 ? newDeadlineMs : noDeadline,
                      std::memory_order_relaxed);
}

void TimingMonitor::reset() noexcept
{
    windowHead = 0;
    windowTail = 0;
    windowCount = 0;
    sequence = 0;

    callbacks.store (0, std::memory_order_relaxed);
    overruns.store (0, std::memory_order_relaxed);
    droppedBlocks.store (0, std::memory_order_relaxed);
    samplesProcessed.store (0, std::memory_order_relaxed);

    lastCallbackMs.store (0.0, std::memory_order_relaxed);
    rollingMaxCallbackMs.store (0.0, std::memory_order_relaxed);
    worstCallbackMs.store (0.0, std::memory_order_relaxed);
    totalCallbackMs.store (0.0, std::memory_order_relaxed);
}

TimingMonitor::CallbackToken TimingMonitor::beginCallback() const noexcept
{
    return { std::chrono::steady_clock::now() };
}

void TimingMonitor::endCallback (CallbackToken token, int numSamplesProcessed) noexcept
{
    const auto elapsed = std::chrono::duration<double, std::milli> (
        std::chrono::steady_clock::now() - token.started).count();

    recordCallback (elapsed, numSamplesProcessed);
}

void TimingMonitor::recordCallback (double elapsedMs, int numSamplesProcessed) noexcept
{
    if (! std::isfinite (elapsedMs) || elapsedMs < 0.0)
        elapsedMs = 0.0;

    if (numSamplesProcessed > 0)
        samplesProcessed.fetch_add (static_cast<std::uint64_t> (numSamplesProcessed),
                                    std::memory_order_relaxed);

    callbacks.fetch_add (1, std::memory_order_relaxed);

    lastCallbackMs.store (elapsedMs, std::memory_order_relaxed);

    if (elapsedMs > worstCallbackMs.load (std::memory_order_relaxed))
        worstCallbackMs.store (elapsedMs, std::memory_order_relaxed);

    totalCallbackMs.store (totalCallbackMs.load (std::memory_order_relaxed) + elapsedMs,
                           std::memory_order_relaxed);

    pushWindow (elapsedMs);

    const double deadline = deadlineMs.load (std::memory_order_relaxed);

    if (deadline <= 0.0 || elapsedMs <= deadline)
        return;

    overruns.fetch_add (1, std::memory_order_relaxed);

    // Only complete extra periods count as dropped blocks. A callback at 1.5x
    // the deadline overruns, but has not yet swallowed a whole block period;
    // 2x drops exactly one, 3x drops two.
    const double periods = elapsedMs / deadline;
    const auto wholePeriods = static_cast<std::uint64_t> (
        std::floor (periods + periodEpsilon));

    if (wholePeriods > 1)
        droppedBlocks.fetch_add (wholePeriods - 1, std::memory_order_relaxed);
}

void TimingMonitor::pushWindow (double milliseconds) noexcept
{
    const auto current = sequence;
    const auto capacity = static_cast<std::uint64_t> (windowSize);

    // Drop entries that leave the window once `current` is added.
    while (windowCount > 0
           && window[static_cast<size_t> (windowHead)].sequence + capacity <= current)
    {
        windowHead = (windowHead + 1) % windowSize;
        --windowCount;
    }

    // Keep the deque monotonic: anything at or below the new value can never
    // be the maximum again while the new value is in the window.
    while (windowCount > 0)
    {
        const int last = (windowTail + windowSize - 1) % windowSize;

        if (window[static_cast<size_t> (last)].milliseconds > milliseconds)
            break;

        windowTail = last;
        --windowCount;
    }

    window[static_cast<size_t> (windowTail)] = { current, milliseconds };
    windowTail = (windowTail + 1) % windowSize;
    ++windowCount;

    ++sequence;

    rollingMaxCallbackMs.store (window[static_cast<size_t> (windowHead)].milliseconds,
                                std::memory_order_relaxed);
}

double TimingMonitor::getDeadlineMs() const noexcept
{
    return deadlineMs.load (std::memory_order_relaxed);
}

std::uint64_t TimingMonitor::getCallbackCount() const noexcept
{
    return callbacks.load (std::memory_order_relaxed);
}

std::uint64_t TimingMonitor::getOverrunCount() const noexcept
{
    return overruns.load (std::memory_order_relaxed);
}

std::uint64_t TimingMonitor::getDroppedBlockCount() const noexcept
{
    return droppedBlocks.load (std::memory_order_relaxed);
}

std::uint64_t TimingMonitor::getSamplesProcessed() const noexcept
{
    return samplesProcessed.load (std::memory_order_relaxed);
}

double TimingMonitor::getLastCallbackMs() const noexcept
{
    return lastCallbackMs.load (std::memory_order_relaxed);
}

double TimingMonitor::getRollingMaxCallbackMs() const noexcept
{
    return rollingMaxCallbackMs.load (std::memory_order_relaxed);
}

double TimingMonitor::getWorstCallbackMs() const noexcept
{
    return worstCallbackMs.load (std::memory_order_relaxed);
}

double TimingMonitor::getAverageCallbackMs() const noexcept
{
    const auto count = callbacks.load (std::memory_order_relaxed);

    if (count == 0)
        return 0.0;

    return totalCallbackMs.load (std::memory_order_relaxed)
           / static_cast<double> (count);
}

double TimingMonitor::getLastLoadPercent() const noexcept
{
    const double deadline = deadlineMs.load (std::memory_order_relaxed);

    if (deadline <= 0.0)
        return 0.0;

    return lastCallbackMs.load (std::memory_order_relaxed) * 100.0 / deadline;
}

TimingSnapshot TimingMonitor::snapshot() const noexcept
{
    TimingSnapshot result;

    result.callbacks = callbacks.load (std::memory_order_relaxed);
    result.overruns = overruns.load (std::memory_order_relaxed);
    result.droppedBlocks = droppedBlocks.load (std::memory_order_relaxed);
    result.samplesProcessed = samplesProcessed.load (std::memory_order_relaxed);

    result.sampleRate = sampleRate.load (std::memory_order_relaxed);
    result.blockSize = blockSize.load (std::memory_order_relaxed);
    result.deadlineMs = deadlineMs.load (std::memory_order_relaxed);

    result.lastCallbackMs = lastCallbackMs.load (std::memory_order_relaxed);
    result.rollingMaxCallbackMs = rollingMaxCallbackMs.load (std::memory_order_relaxed);
    result.worstCallbackMs = worstCallbackMs.load (std::memory_order_relaxed);
    result.averageCallbackMs = getAverageCallbackMs();
    result.lastLoadPercent = getLastLoadPercent();

    result.windowSize = windowSize;

    return result;
}

} // namespace AudioEngine
