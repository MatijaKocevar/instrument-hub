#include "TestSupport.h"
#include "driver/ArmingCapture.h"
#include "driver/Grid.h"
#include "driver/Handshake.h"
#include "driver/LedStream.h"
#include "driver/PatchLeds.h"
#include "driver/Rig.h"

#include <juce_events/juce_events.h>

#include <iostream>
#include <string>

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    DriverTest::Rig rig;
    std::string error;

    ControllerTest::check (rig.load (error),
                           std::string ("shared/set.yaml loads")
                               + (error.empty() ? std::string() : " (" + error + ")"));

    DriverTest::runHandshakeChecks (rig);
    DriverTest::runLedStreamChecks (rig);
    DriverTest::runGridChecks (rig);
    DriverTest::runPatchLedChecks (rig);
    DriverTest::runArmingCaptureChecks (rig);

    std::cout << (ControllerTest::failures == 0 ? "\nPASS" : "\nFAIL") << ": "
              << ControllerTest::failures << " failed check(s)" << std::endl;

    return ControllerTest::failures == 0 ? 0 : 1;
}
