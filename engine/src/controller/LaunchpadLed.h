#pragma once

#include "LaunchpadProtocol.h"
#include "PadState.h"

#include <cstdint>

namespace Controller
{

/** The LED colour used for a pad state on the Launchpad. */
Launchpad::Colour launchpadColourForState (PadState state) noexcept;

/** One pad LED: a raw palette velocity plus the lighting mode. */
struct PatchLed
{
    std::uint8_t velocity = 0;
    Launchpad::LedMode mode = Launchpad::LedMode::Static;

    bool operator== (const PatchLed&) const = default;
};

/**
    The grid's LED policy.

    - a patch being warmed is yellow,
    - a load error flashes red,
    - the armed patch is bright red,
    - every other patch shows its palette colour.

    Warming wins over armed so a cold arm is visible while it loads. The
    function is pure: the driver and its scripts share one policy.
*/
PatchLed launchpadLedForPatch (int colorIndex,
                               bool armed,
                               bool warming,
                               bool error) noexcept;

} // namespace Controller
