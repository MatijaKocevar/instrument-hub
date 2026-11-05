#include "sandbox/TestSupport.h"
#include "sandbox/CrashIsolation.h"
#include "sandbox/PageHostScenario.h"
#include "sandbox/RealPlugin.h"
#include "sandbox/Transport.h"

#include <juce_events/juce_events.h>

#include <iostream>

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    std::cout << "ihub-sandbox-test" << std::endl;
    std::cout << "helper: " << IHUB_SANDBOX_HELPER << std::endl;

    SandboxTest::runTransport();
    SandboxTest::runCrashIsolation();
    SandboxTest::runRealPlugin();
    SandboxTest::runPageHostTransport();
    SandboxTest::runPageHostRecovery();
    SandboxTest::runPageHostFailedRestart();

    if (SandboxTest::failures == 0)
    {
        std::cout << "\nPASS: sandbox transport, crash isolation, real plugin hosting "
                     "and shared page recovery"
                  << std::endl;
        return 0;
    }

    std::cerr << "\n" << SandboxTest::failures << " check(s) failed" << std::endl;
    return 1;
}
