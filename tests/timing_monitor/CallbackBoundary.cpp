#include "CallbackBoundary.h"
#include "TestSupport.h"

#include "audio/TimingMonitor.h"

#include <iostream>

namespace TimingMonitorTest
{

void runCallbackBoundary()
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

    monitor.recordCallback (deadline, 128);

    check (monitor.getOverrunCount() == 0,
           "a callback exactly at the deadline is not an overrun");

    monitor.recordCallback (deadline * 1.01, 128);

    check (monitor.getOverrunCount() == 1,
           "a callback past the deadline is an overrun");
    check (monitor.getDroppedBlockCount() == 0,
           "an overrun below 2x the deadline drops no whole block");
    check (monitor.getSamplesProcessed() == 3 * 128,
           "processed samples are accumulated");
}

}
