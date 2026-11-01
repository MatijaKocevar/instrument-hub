#pragma once

#include "LaunchpadProtocol.h"
#include "PadState.h"

#include <cstdint>

namespace Controller
{

Launchpad::Colour launchpadColourForState (PadState state) noexcept;

struct PatchLed
{
    std::uint8_t velocity = 0;
    Launchpad::LedMode mode = Launchpad::LedMode::Static;

    bool operator== (const PatchLed&) const = default;
};

PatchLed launchpadLedForPatch (int colorIndex,
                               bool armed,
                               bool warming,
                               bool error) noexcept;

}
