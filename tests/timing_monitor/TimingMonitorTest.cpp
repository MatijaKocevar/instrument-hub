// ihub-timing-monitor-test: acceptance check for T013.
//
// The central test injects a deliberate stall of exactly twice the block
// deadline through TimingMonitor::recordCallback(), which is the deterministic
// seam for a pre-measured callback duration. It asserts that the overrun (xrun)
// counter increments exactly once per stall and that the stall drops exactly
// one block. No wall-clock sleeping is involved, so the result cannot flake.

#include "audio/TimingMonitor.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>

namespace
{
int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

bool nearlyEqual (double a, double b, double tolerance)
{
    return std::abs (a - b) <= tolerance;
}

void testDeadlineFromDeviceFormat()
{
    std::cout << "\n-- deadline follows the device format --" << std::endl;

    AudioEngine::TimingMonitor monitor;

    check (monitor.getWindowSize() == AudioEngine::TimingMonitor::defaultWindowSize,
           "default rolling window is 512 callbacks");

    monitor.prepare (48000.0, 128);

    const double expectedMs = 128.0 * 1000.0 / 48000.0;

    std::cout << "deadline " << monitor.getDeadlineMs() << " ms (expected "
              << expectedMs << " ms)" << std::endl;

    check (nearlyEqual (monitor.getDeadlineMs(), expectedMs, 1.0e-12),
           "128 frames at 48 kHz gives a 2.667 ms deadline");

    const auto snapshot = monitor.snapshot();

    check (nearlyEqual (snapshot.sampleRate, 48000.0, 0.0) && snapshot.blockSize == 128,
           "snapshot reports the prepared sample rate and block size");
    check (snapshot.windowSize == 512, "snapshot reports the rolling window size");

    monitor.setDeadlineMs (4.0);
    check (nearlyEqual (monitor.getDeadlineMs(), 4.0, 1.0e-12),
           "setDeadlineMs overrides the derived deadline");

    monitor.setDeadlineMs (-1.0);
    check (nearlyEqual (monitor.getDeadlineMs(), 0.0, 0.0),
           "a non-positive deadline disables overrun counting");
}

void testNormalCallbackAndBoundary()
{
    std::cout << "\n-- normal callbacks and the deadline boundary --" << std::endl;

    AudioEngine::TimingMonitor monitor;
    monitor.prepare (48000.0, 128);

    const double deadline = monitor.getDeadlineMs();

    monitor.recordCallback (deadline * 0.5, 128);

    check (monitor.getCallbackCount() == 1, "a normal callback is counted");
    check (monitor.getOverrunCount() == 0, "a normal callback is not an overrun");
    check (monitor.getDroppedBlockCount() == 0, "a normal callback drops no blocks");
    check (nearlyEqual (monitor.getLastCallbackMs(), deadline * 0.5, 1.0e-12),
           "the last callback duration is recorded");
    check (nearlyEqual (monitor.getLastLoadPercent(), 50.0, 1.0e-9),
           "load is reported relative to the deadline");

    // Exactly at the deadline is still on time.
    monitor.recordCallback (deadline, 128);

    check (monitor.getOverrunCount() == 0,
           "a callback exactly at the deadline is not an overrun");

    // Slightly over the deadline is an overrun, but has not swallowed a whole
    // extra block period yet.
    monitor.recordCallback (deadline * 1.01, 128);

    check (monitor.getOverrunCount() == 1,
           "a callback past the deadline is an overrun");
    check (monitor.getDroppedBlockCount() == 0,
           "an overrun below 2x the deadline drops no whole block");
    check (monitor.getSamplesProcessed() == 3 * 128,
           "processed samples are accumulated");
}

void testTwoTimesDeadlineStall()
{
    std::cout << "\n-- deliberate 2x deadline stall --" << std::endl;

    constexpr int stallCount = 5;

    AudioEngine::TimingMonitor monitor;
    monitor.prepare (48000.0, 128);

    const double deadline = monitor.getDeadlineMs();

    std::cout << "injecting " << stallCount << " stalls of "
              << deadline * 2.0 << " ms against a " << deadline
              << " ms deadline" << std::endl;

    for (int stall = 0; stall < stallCount; ++stall)
    {
        // A healthy callback in between proves the counters are not just
        // accumulating one long run of overruns.
        monitor.recordCallback (deadline * 0.25, 128);

        const auto overrunsBefore = monitor.getOverrunCount();
        const auto droppedBefore = monitor.getDroppedBlockCount();
        const auto callbacksBefore = monitor.getCallbackCount();

        monitor.recordCallback (deadline * 2.0, 128);

        const auto overrunDelta = monitor.getOverrunCount() - overrunsBefore;
        const auto droppedDelta = monitor.getDroppedBlockCount() - droppedBefore;
        const auto callbackDelta = monitor.getCallbackCount() - callbacksBefore;

        check (overrunDelta == 1,
               "stall " + std::to_string (stall + 1)
                   + ": overrun counter increments exactly once");
        check (droppedDelta == 1,
               "stall " + std::to_string (stall + 1)
                   + ": dropped-block counter increments exactly once");
        check (callbackDelta == 1,
               "stall " + std::to_string (stall + 1) + ": callback is counted once");
    }

    check (monitor.getOverrunCount() == static_cast<std::uint64_t> (stallCount),
           "five stalls produce exactly five overruns");
    check (monitor.getDroppedBlockCount() == static_cast<std::uint64_t> (stallCount),
           "five stalls produce exactly five dropped blocks");
    check (monitor.getCallbackCount() == static_cast<std::uint64_t> (stallCount * 2),
           "both healthy callbacks and stalls are counted");
    check (nearlyEqual (monitor.getWorstCallbackMs(), deadline * 2.0, 1.0e-12),
           "worst callback is the injected stall");
    check (nearlyEqual (monitor.getRollingMaxCallbackMs(), deadline * 2.0, 1.0e-12),
           "rolling maximum reports the injected stall");
    check (nearlyEqual (monitor.getLastCallbackMs(), deadline * 2.0, 1.0e-12),
           "last callback is the injected stall");
    check (monitor.snapshot().overruns == static_cast<std::uint64_t> (stallCount),
           "snapshot exposes the same overrun count");
}

void testDroppedBlockScaling()
{
    std::cout << "\n-- dropped blocks scale with the overrun length --" << std::endl;

    AudioEngine::TimingMonitor monitor;
    monitor.setDeadlineMs (10.0);

    monitor.recordCallback (25.0);

    check (monitor.getOverrunCount() == 1, "a 2.5x callback is one overrun");
    check (monitor.getDroppedBlockCount() == 1,
           "a 2.5x callback drops one whole block");

    monitor.recordCallback (30.0);

    check (monitor.getOverrunCount() == 2, "a 3x callback is one overrun");
    check (monitor.getDroppedBlockCount() == 3,
           "a 3x callback drops two more blocks (three total)");
}

void testRollingWindow()
{
    std::cout << "\n-- rolling maximum over a bounded window --" << std::endl;

    AudioEngine::TimingMonitor monitor (4);
    monitor.setDeadlineMs (10.0);

    monitor.recordCallback (1.0);
    monitor.recordCallback (2.0);
    monitor.recordCallback (3.0);
    monitor.recordCallback (4.0);

    check (nearlyEqual (monitor.getRollingMaxCallbackMs(), 4.0, 1.0e-12),
           "rolling maximum is 4.0 once the window is full");

    monitor.recordCallback (0.5);
    check (nearlyEqual (monitor.getRollingMaxCallbackMs(), 4.0, 1.0e-12),
           "younger small callbacks do not evict the maximum yet");

    monitor.recordCallback (0.5);
    monitor.recordCallback (0.5);
    check (nearlyEqual (monitor.getRollingMaxCallbackMs(), 4.0, 1.0e-12),
           "maximum stays while the 4.0 callback is inside the window");

    monitor.recordCallback (0.5);
    check (nearlyEqual (monitor.getRollingMaxCallbackMs(), 0.5, 1.0e-12),
           "maximum falls to 0.5 after 4.0 leaves the window");
    check (nearlyEqual (monitor.getWorstCallbackMs(), 4.0, 1.0e-12),
           "all-time worst callback is not forgotten");
}

void testMeasuredCallback()
{
    std::cout << "\n-- measured begin/end path --" << std::endl;

    AudioEngine::TimingMonitor monitor;
    monitor.prepare (48000.0, 512);

    const auto callbacksBefore = monitor.getCallbackCount();

    const auto token = monitor.beginCallback();

    // A tiny amount of deterministic work, just enough that the clock advances.
    volatile double accumulator = 0.0;

    for (int i = 0; i < 1000; ++i)
        accumulator = accumulator + static_cast<double> (i);

    monitor.endCallback (token, 512);

    const double lastMs = monitor.getLastCallbackMs();

    std::cout << "measured callback " << lastMs << " ms against a "
              << monitor.getDeadlineMs() << " ms deadline" << std::endl;

    check (monitor.getCallbackCount() == callbacksBefore + 1,
           "begin/end records exactly one callback");
    check (std::isfinite (lastMs) && lastMs >= 0.0,
           "measured callback duration is finite and non-negative");
    check (lastMs < monitor.getDeadlineMs(),
           "the trivial measured callback is well inside the deadline");
    check (monitor.getSamplesProcessed() == 512,
           "endCallback records the processed sample count");
}

void testReset()
{
    std::cout << "\n-- reset clears counters but keeps the deadline --" << std::endl;

    AudioEngine::TimingMonitor monitor;
    monitor.prepare (48000.0, 128);

    monitor.recordCallback (monitor.getDeadlineMs() * 2.0, 128);

    check (monitor.getOverrunCount() == 1, "reset test starts with one overrun");

    monitor.reset();

    check (monitor.getCallbackCount() == 0
               && monitor.getOverrunCount() == 0
               && monitor.getDroppedBlockCount() == 0
               && monitor.getSamplesProcessed() == 0,
           "counters are cleared");
    check (nearlyEqual (monitor.getLastCallbackMs(), 0.0, 0.0)
               && nearlyEqual (monitor.getRollingMaxCallbackMs(), 0.0, 0.0)
               && nearlyEqual (monitor.getWorstCallbackMs(), 0.0, 0.0),
           "timing history is cleared");
    check (monitor.getDeadlineMs() > 0.0, "the configured deadline survives reset");
}
} // namespace

int main()
{
    std::cout << "ihub-timing-monitor-test: T013 xrun and timing monitor" << std::endl;

    testDeadlineFromDeviceFormat();
    testNormalCallbackAndBoundary();
    testTwoTimesDeadlineStall();
    testDroppedBlockScaling();
    testRollingWindow();
    testMeasuredCallback();
    testReset();

    std::cout << (failures == 0 ? "\nPASS" : "\nFAIL")
              << ": ihub-timing-monitor (" << failures << " failure(s))"
              << std::endl;

    return failures == 0 ? 0 : 1;
}
