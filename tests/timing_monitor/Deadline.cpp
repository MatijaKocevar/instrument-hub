#include "Deadline.h"
#include "TestSupport.h"

#include "audio/TimingMonitor.h"

#include <iostream>

namespace TimingMonitorTest
{

void runDeadline()
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

} // namespace TimingMonitorTest
