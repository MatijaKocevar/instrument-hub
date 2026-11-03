#include "ArmingCapture.h"

#include "../TestSupport.h"
#include "ArmingCaptureGrid.h"

#include "controller/LaunchpadProtocol.h"

#include <string>

namespace DriverTest
{

using ControllerTest::check;

void runArmingCaptureChecks (Rig& rig)
{
    auto before = makeGrid();
    const auto beforeLeds = paint (rig, before);

    before.armedSlot = captureArmedPad;
    rig.sent.clear();
    const auto afterLeds = paint (rig, before);

    int changedPad = -1;
    const int differences = countDifferences (beforeLeds, afterLeds, changedPad);

    check (differences == 1 && changedPad == captureArmedPad,
           "arming one pad changes exactly one LED (pad "
               + std::to_string (captureArmedPad) + ")");
    check (afterLeds[static_cast<std::size_t> (captureArmedPad)].velocity
               == Launchpad::velocityFor (Launchpad::Colour::Red)
               && afterLeds[static_cast<std::size_t> (captureArmedPad)].mode
                      == Launchpad::LedMode::Static,
           "the armed pad keeps a static red LED");
    check (rig.sent.size() == 1
               && isFrameFor (rig.sent.front (), captureArmedPad,
                              Launchpad::LedMode::Static,
                              Launchpad::velocityFor (Launchpad::Colour::Red)),
           "arming sends exactly one LED frame, for the armed pad only");

    bool othersKeepTheirColour = true;

    for (int pad = 0; pad < Launchpad::padCount; ++pad)
    {
        if (pad == captureArmedPad)
            continue;

        const auto index = static_cast<std::size_t> (pad);

        if (afterLeds[index].velocity
                != Launchpad::velocityForPatchColour (pad % 16)
            || afterLeds[index].mode != Launchpad::LedMode::Static)
            othersKeepTheirColour = false;
    }

    check (othersKeepTheirColour,
           "every other pad keeps its patch colour");

    auto warmingArmed = before;
    warmingArmed.pads[static_cast<std::size_t> (captureArmedPad)].warming = true;
    warmingArmed.pads[static_cast<std::size_t> (captureArmedPad - 1)].warming = true;
    warmingArmed.pads[static_cast<std::size_t> (captureArmedPad + 1)].warming = true;
    rig.sent.clear();
    const auto warmingArmedLeds = paint (rig, warmingArmed);

    check (countDifferences (afterLeds, warmingArmedLeds, changedPad) == 1
               && changedPad == captureArmedPad
               && warmingArmedLeds[static_cast<std::size_t> (captureArmedPad)].velocity
                      == Launchpad::velocityFor (Launchpad::Colour::Red)
               && warmingArmedLeds[static_cast<std::size_t> (captureArmedPad)].mode
                      == Launchpad::LedMode::Pulsing,
           "a cold armed pad pulses red without touching the other pads");
    check (rig.sent.size() == 1
               && isFrameFor (rig.sent.front (), captureArmedPad,
                              Launchpad::LedMode::Pulsing,
                              Launchpad::velocityFor (Launchpad::Colour::Red)),
           "the warming armed pad sends one red pulsing frame");

    auto warming = warmingArmed;
    warming.pads[static_cast<std::size_t> (captureWarmingPad)].warming = true;
    rig.sent.clear();
    const auto warmingLeds = paint (rig, warming);

    check (countDifferences (warmingArmedLeds, warmingLeds, changedPad) == 0,
           "background warming does not repaint another pad");
    check (warmingLeds[static_cast<std::size_t> (captureWarmingPad)].velocity
               == Launchpad::velocityForPatchColour (captureWarmingPad)
               && warmingLeds[static_cast<std::size_t> (captureWarmingPad)].mode
                      == Launchpad::LedMode::Static,
           "a non-armed warming pad keeps its static patch colour");
    check (rig.sent.empty (),
           "a non-armed warm-state flip sends no LED frame");

    auto failed = warming;
    failed.pads[static_cast<std::size_t> (captureWarmingPad)].warming = false;
    failed.pads[static_cast<std::size_t> (captureWarmingPad)].error = true;
    rig.sent.clear();
    const auto failedLeds = paint (rig, failed);

    check (failedLeds[static_cast<std::size_t> (captureWarmingPad)].velocity
               == Launchpad::velocityFor (Launchpad::Colour::Red)
               && failedLeds[static_cast<std::size_t> (captureWarmingPad)].mode
                      == Launchpad::LedMode::Flashing,
           "an erroring pad still flashes red");
    check (rig.sent.size() == 1
               && isFrameFor (rig.sent.front (), captureWarmingPad,
                              Launchpad::LedMode::Flashing,
                              Launchpad::velocityFor (Launchpad::Colour::Red)),
           "the error frame flashes red on the failing pad only");
}

}
