#include "TestSupport.h"
#include "track_pads/Capture.h"
#include "track_pads/Leds.h"
#include "track_pads/Routing.h"

#include <juce_events/juce_events.h>

#include <iostream>

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    TrackPadTest::runRoutingChecks();
    TrackPadTest::runLedChecks();
    TrackPadTest::runCaptureChecks();

    std::cout << (ControllerTest::failures == 0 ? "\nPASS" : "\nFAIL") << ": "
              << ControllerTest::failures << " failed check(s)" << std::endl;

    return ControllerTest::failures == 0 ? 0 : 1;
}
