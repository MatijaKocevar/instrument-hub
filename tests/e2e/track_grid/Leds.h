#pragma once

#include "api/StateStore.h"
#include "controller/LaunchpadDriver.h"
#include "controller/Mapping.h"

#include <cstdint>
#include <vector>

namespace TrackGridE2E
{

struct LedEvent
{
    int pad = -1;
    std::uint8_t velocity = 0;
    int mode = -1;
};

void attachLedStream (Controller::LaunchpadDriver& driver,
                      std::vector<LedEvent>& events);

void paintTrackLeds (Api::StateStore& state,
                     const Controller::Mapping& mapping,
                     Controller::LaunchpadDriver& driver);

int countRedTrackLeds (Controller::LaunchpadDriver& driver);

bool ledIs (Controller::LaunchpadDriver& driver,
            int pad,
            Launchpad::Colour colour);

}
