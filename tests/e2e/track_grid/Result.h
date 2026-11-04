#pragma once

#include "track_grid/Memory.h"

#include <cstdint>
#include <string>

namespace TrackGridE2E
{

struct TrackGridResult
{
    bool ran = false;
    std::string error;

    int trackCount = 0;
    int protectedCount = 0;
    std::uint64_t warmCreated = 0;
    int liveAfterWarm = 0;
    int peakAfterWarm = 0;

    ProcessMemory beforeWarm;
    ProcessMemory afterWarm;

    int switches = 0;
    int warmingSeen = 0;
    std::uint64_t creationAfterSwitches = 0;
    std::uint64_t evictionsAfterSwitches = 0;

    int fxToggles = 0;
    int volumeSets = 0;

    int redTrackLeds = 0;
    int ledEvents = 0;

    std::uint64_t callbacks = 0;
    std::uint64_t xruns = 0;
    double worstCallbackMs = 0.0;
    double deadlineMs = 0.0;
    double lastSwitchRmsDbfs = -1000.0;
};

}
