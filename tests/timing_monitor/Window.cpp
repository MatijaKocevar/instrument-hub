#include "Window.h"
#include "TestSupport.h"

#include "audio/TimingMonitor.h"

#include <iostream>

namespace TimingMonitorTest
{

void runWindow()
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

}
