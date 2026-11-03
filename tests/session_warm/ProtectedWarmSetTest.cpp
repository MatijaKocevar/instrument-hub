#include "ProtectedWarmFixtures.h"

#include <juce_events/juce_events.h>

#include <iostream>

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    std::cout << "ihub-session-warm-protected-test: eight protected slots" << std::endl;

    ProtectedWarmTest::runWarmPoolScenario();
    ProtectedWarmTest::runSessionScenario();

    std::cout << (ProtectedWarmTest::failures == 0 ? "\nPASS" : "\nFAIL")
              << ": " << ProtectedWarmTest::failures << " failed check(s)" << std::endl;

    return ProtectedWarmTest::failures == 0 ? 0 : 1;
}
