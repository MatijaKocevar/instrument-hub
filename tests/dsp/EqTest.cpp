#include "TestSupport.h"
#include "eq/Flat.h"
#include "eq/Realtime.h"
#include "eq/Response.h"
#include "eq/State.h"

#include <juce_events/juce_events.h>

#include <iostream>

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    DspTest::Eq::runFlat();
    DspTest::Eq::runResponse();
    DspTest::Eq::runRealtime();
    DspTest::Eq::runState();

    std::cout << (DspTest::failures == 0 ? "\nPASS" : "\nFAIL") << ": "
              << DspTest::failures << " failed check(s)" << std::endl;

    return DspTest::failures == 0 ? 0 : 1;
}
