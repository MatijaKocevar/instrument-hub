#include "Stall.h"
#include "TestSupport.h"

#include "audio/TimingMonitor.h"

#include <cstdint>
#include <iostream>
#include <string>

namespace TimingMonitorTest
{

void runStall()
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

}
