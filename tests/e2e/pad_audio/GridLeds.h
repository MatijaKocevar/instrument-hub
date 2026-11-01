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

std::map<int, LedHit> captureGridLeds (const Api::StateSnapshot& snapshot,
                                       Controller::LaunchpadDriver& driver);

}
