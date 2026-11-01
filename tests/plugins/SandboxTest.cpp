#include "sandbox/TestSupport.h"
#include "sandbox/CrashIsolation.h"
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

    if (SandboxTest::failures == 0)
    {
        std::cout << "\nPASS: sandbox transport, crash isolation and real plugin hosting"
                  << std::endl;
        return 0;
    }

    std::cerr << "\n" << SandboxTest::failures << " check(s) failed" << std::endl;
    return 1;
}
