#include "Leds.h"

#include "../TestSupport.h"

#include "Hex.h"
#include "controller/LaunchpadProtocol.h"

#include <array>
#include <cstdint>

namespace ProtocolTest
{

using ControllerTest::check;

void testLeds()
{
    check (Launchpad::paletteVelocitiesAreDistinct(),
           "every named palette colour has a distinct velocity");

    check (Launchpad::velocityFor (Launchpad::Colour::Off) == 0, "off is velocity 0");
    check (Launchpad::velocityFor (Launchpad::Colour::Red) == 5, "red is velocity 5");
    check (Launchpad::velocityFor (Launchpad::Colour::Green) == 21, "green is velocity 21");
    check (Launchpad::velocityFor (Launchpad::Colour::Blue) == 45, "blue is velocity 45");

    const auto redVelocity = Launchpad::velocityFor (Launchpad::Colour::Red);
    bool anyRedPatchColour = false;
    bool distinctPatchPalette = true;

    for (int index = 0; index < 16; ++index)
    {
        if (Launchpad::velocityForPatchColour (index) == redVelocity)
            anyRedPatchColour = true;

        for (int other = index + 1; other < 16; ++other)
            if (Launchpad::velocityForPatchColour (index)
                == Launchpad::velocityForPatchColour (other))
                distinctPatchPalette = false;
    }

    check (! anyRedPatchColour,
           "no patch palette velocity is the armed red (16-index sweep)");
    check (distinctPatchPalette,
           "all 16 patch palette velocities are distinct");

    const auto bottomLeft = Launchpad::padColourMessage (0, Launchpad::Colour::Green);
    check (bottomLeft == std::array<std::uint8_t, 3> { 0x90, 0x0B, 0x15 },
           "pad 0 green is 90 0B 15, got " + hexBytes (bottomLeft));

    const auto topRight = Launchpad::padColourMessage (63, Launchpad::Colour::Green);
    check (topRight == std::array<std::uint8_t, 3> { 0x90, 0x58, 0x15 },
           "pad 63 green is 90 58 15, got " + hexBytes (topRight));

    const auto flashing = Launchpad::padColourMessage (0, Launchpad::Colour::Red,
                                                       Launchpad::LedMode::Flashing);
    check (flashing == std::array<std::uint8_t, 3> { 0x91, 0x0B, 0x05 },
           "flashing red uses status 0x91");

    const auto pulsing = Launchpad::padColourMessage (0, Launchpad::Colour::Red,
                                                      Launchpad::LedMode::Pulsing);
    check (pulsing == std::array<std::uint8_t, 3> { 0x92, 0x0B, 0x05 },
           "pulsing red uses status 0x92");

    const auto invalid = Launchpad::padColourMessage (64, Launchpad::Colour::Green);
    check (invalid == std::array<std::uint8_t, 3> { 0, 0, 0 },
           "an out-of-range pad produces no LED message");
}

}
