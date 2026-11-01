#include "TestSupport.h"
#include "CallbackBoundary.h"
#include "Deadline.h"
#include "DroppedBlocks.h"
#include "Measured.h"
#include "Reset.h"
#include "Stall.h"
#include "Window.h"

#include <iostream>

int main()
{
    std::cout << "ihub-timing-monitor-test: T013 xrun and timing monitor" << std::endl;

    TimingMonitorTest::runDeadline();
    TimingMonitorTest::runCallbackBoundary();
    TimingMonitorTest::runStall();
    TimingMonitorTest::runDroppedBlocks();
    TimingMonitorTest::runWindow();
    TimingMonitorTest::runMeasured();
    TimingMonitorTest::runReset();

    std::cout << (TimingMonitorTest::failures == 0 ? "\nPASS" : "\nFAIL")
              << ": ihub-timing-monitor (" << TimingMonitorTest::failures << " failure(s))"
              << std::endl;

    return TimingMonitorTest::failures == 0 ? 0 : 1;
}
