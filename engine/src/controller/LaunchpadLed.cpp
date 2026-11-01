#include "LaunchpadLed.h"

namespace Controller
{

Launchpad::Colour launchpadColourForState (PadState state) noexcept
{
    switch (state)
    {
        case PadState::idle:    return Launchpad::Colour::Off;
        case PadState::warming: return Launchpad::Colour::Yellow;
        case PadState::active:  return Launchpad::Colour::Green;
        case PadState::error:   return Launchpad::Colour::Red;
    }

    return Launchpad::Colour::Off;
}

PatchLed launchpadLedForPatch (int colorIndex,
                               bool armed,
                               bool warming,
                               bool error) noexcept
{
    if (error)
        return { Launchpad::velocityFor (Launchpad::Colour::Red),
                 Launchpad::LedMode::Flashing };

    if (warming)
        return { Launchpad::velocityFor (Launchpad::Colour::Yellow),
                 Launchpad::LedMode::Static };

    if (armed)
        return { Launchpad::velocityFor (Launchpad::Colour::Red),
                 Launchpad::LedMode::Static };

    return { Launchpad::velocityForPatchColour (colorIndex),
             Launchpad::LedMode::Static };
}

} // namespace Controller
