#include "Measured.h"
#include "TestSupport.h"

#include "audio/TimingMonitor.h"

#include <cmath>
#include <iostream>

namespace TimingMonitorTest
{

void runMeasured()
{
    std::cout << "\n-- measured begin/end path --" << std::endl;

    AudioEngine::TimingMonitor monitor;
    monitor.prepare (48000.0, 512);

    const auto callbacksBefore = monitor.getCallbackCount();

    const auto token = monitor.beginCallback();

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

} // namespace TimingMonitorTest
