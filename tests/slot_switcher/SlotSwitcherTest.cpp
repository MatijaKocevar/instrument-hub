#include "Fixtures.h"
#include "Continuity.h"
#include "Disarm.h"
#include "Realtime.h"

#include <juce_events/juce_events.h>

#include <iostream>

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    SlotSwitcherTest::runContinuity();
    SlotSwitcherTest::runNoAllocations();
    SlotSwitcherTest::runDisarm();

    std::cout << (SlotSwitcherTest::failures == 0 ? "\nPASS" : "\nFAIL")
              << ": " << SlotSwitcherTest::failures
              << " failed check(s)" << std::endl;

    return SlotSwitcherTest::failures == 0 ? 0 : 1;
}
