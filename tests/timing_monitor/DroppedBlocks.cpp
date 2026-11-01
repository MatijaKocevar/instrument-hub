#include "DroppedBlocks.h"
#include "TestSupport.h"

#include "audio/TimingMonitor.h"

#include <iostream>

namespace TimingMonitorTest
{

void runDroppedBlocks()
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

} // namespace TimingMonitorTest
