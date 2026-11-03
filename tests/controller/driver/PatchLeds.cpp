#include "PatchLeds.h"

#include "../TestSupport.h"

#include "controller/LaunchpadProtocol.h"

namespace DriverTest
{

using ControllerTest::check;

void runPatchLedChecks (Rig& rig)
{
    rig.sent.clear();

    const auto idle = Controller::launchpadLedForPatch (2, false, false, false);
    const auto armed = Controller::launchpadLedForPatch (9, true, false, false);
    const auto armedWarming = Controller::launchpadLedForPatch (9, true, true, false);
    const auto warming = Controller::launchpadLedForPatch (9, false, true, false);
    const auto errorLed = Controller::launchpadLedForPatch (9, false, false, true);

    check (idle.velocity == Launchpad::velocityForPatchColour (2)
               && idle.mode == Launchpad::LedMode::Static,
           "an unarmed patch shows its palette colour");
    check (armed.velocity == Launchpad::velocityFor (Launchpad::Colour::Red)
               && armed.mode == Launchpad::LedMode::Static,
           "the armed patch is bright red");
    check (armedWarming.velocity == Launchpad::velocityFor (Launchpad::Colour::Red)
               && armedWarming.mode == Launchpad::LedMode::Pulsing,
           "an armed patch that is warming pulses red");
    check (warming.velocity == Launchpad::velocityForPatchColour (9)
               && warming.mode == Launchpad::LedMode::Pulsing,
           "a warming patch keeps its colour and pulses");
    check (errorLed.velocity == Launchpad::velocityFor (Launchpad::Colour::Red)
               && errorLed.mode == Launchpad::LedMode::Flashing,
           "a patch error flashes red");

    bool distinctPalette = true;
    bool idlePaletteIsNeverRed = true;
    const auto redVelocity = Launchpad::velocityFor (Launchpad::Colour::Red);

    for (int index = 0; index < 16; ++index)
    {
        if (Controller::launchpadLedForPatch (index, false, false, false).velocity
            == redVelocity)
            idlePaletteIsNeverRed = false;

        for (int other = index + 1; other < 16; ++other)
            if (Launchpad::velocityForPatchColour (index)
                == Launchpad::velocityForPatchColour (other))
                distinctPalette = false;
    }

    check (distinctPalette, "all 16 patch colours have distinct LED velocities");
    check (idlePaletteIsNeverRed,
           "an unarmed patch never uses the armed red LED");

    rig.driver.setPatchLed (0, 2, false, false, false);

    check (! rig.sent.empty ()
               && hex (rig.sent.back()) == "90 0B " + velocityHex (
                      Launchpad::velocityForPatchColour (2)),
           "the driver lights an unarmed patch with its palette colour");

    rig.driver.setPatchLed (0, 9, false, false, false);

    check (! rig.sent.empty ()
               && hex (rig.sent.back()) == "90 0B " + velocityHex (
                      Launchpad::velocityForPatchColour (9)),
           "changing the patch colour sends the new palette LED");

    rig.driver.setPatchLed (0, 9, true, false, false);
    check (hex (rig.sent.back()) == "90 0B 05", "the armed pad LED is red");

    rig.driver.setPatchLed (0, 9, true, true, false);
    check (hex (rig.sent.back()) == "92 0B 05",
           "the armed pad pulses red while it warms");

    rig.driver.setPatchLed (0, 9, false, true, false);
    check (hex (rig.sent.back()) == "92 0B " + velocityHex (
                      Launchpad::velocityForPatchColour (9)),
           "the warming pad LED pulses its patch colour");

    rig.driver.setPatchLed (0, 9, false, false, true);
    check (hex (rig.sent.back()) == "91 0B 05", "the error pad LED flashes red");
}

}
