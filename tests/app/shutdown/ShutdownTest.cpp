#include "Scenarios.h"
#include "TestSupport.h"

#include <juce_events/juce_events.h>

#include <iostream>
#include <string>

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    std::cout << "ihub-shutdown-test: session and control API teardown" << std::endl;

    const double apiShutdown = ShutdownTest::runSessionApiShutdown();

    ShutdownTest::check (apiShutdown < 2000.0,
                         "session + control API stop in under 2 s ("
                             + std::to_string (apiShutdown) + " ms)");

    ShutdownTest::simulateStoppedMessageLoop();

    const double stalledShutdown = ShutdownTest::runStalledLoadShutdown();

    ShutdownTest::check (stalledShutdown < 2000.0,
                         "an in-flight sandbox load is cancelled in under 2 s ("
                             + std::to_string (stalledShutdown) + " ms)");

    if (ShutdownTest::failures > 0)
    {
        std::cerr << "ihub-shutdown-test: " << ShutdownTest::failures
                  << " check(s) failed" << std::endl;
        return 1;
    }

    std::cout << "ihub-shutdown-test: all checks passed" << std::endl;
    return 0;
}
