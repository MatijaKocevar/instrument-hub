#include "Reset.h"
#include "TestSupport.h"

#include "audio/TimingMonitor.h"

#include <iostream>

namespace TimingMonitorTest
{

void runReset()
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

}
