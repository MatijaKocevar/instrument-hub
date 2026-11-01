#pragma once

#include "api/StateStore.h"
#include "controller/LaunchpadDriver.h"

#include <cstdint>
#include <map>

namespace PadAudio
{

struct LedHit
{
    bool seen = false;
    std::uint8_t velocity = 0;
    int mode = -1;
};

/** Applies the Grid page's LED policy and captures the emitted MIDI.

    This is the exact policy `Main::refreshControllerLeds()` applies on every
    API change and 100 ms poll: the armed pad is red, a warming pad yellow, an
    error flashes red, and every other pad shows its patch colour.
*/
std::map<int, LedHit> captureGridLeds (const Api::StateSnapshot& snapshot,
                                       Controller::LaunchpadDriver& driver);

} // namespace PadAudio
