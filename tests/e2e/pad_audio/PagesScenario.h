#pragma once

#include "pad_audio/GridLeds.h"

#include <cstdint>
#include <string>

namespace PadAudio
{

struct PagesResult
{
    bool ran = false;
    std::string error;
    int clickSlot = -1;
    int clickArmed = -1;
    int pad = -1;
    int padArmed = -1;
    int patchColorIndex = -1;
    double clickRmsDbfs = -1000.0;
    double padRmsDbfs = -1000.0;
    LedHit armedLed;
    LedHit idleLed;
    std::uint8_t idleExpected = 0;
    bool samePatch = false;
};

PagesResult runPagesScenario();

}
