#pragma once

#include "PatchState.h"

#include <string>
#include <vector>

namespace Api
{

inline constexpr int trackVolumeMinimumStep = 0;
inline constexpr int trackVolumeMaximumStep = 5;
inline constexpr int trackVolumeDefaultStep = 4;

struct TrackHotFxState
{
    int index = 0;
    std::string id;
    std::string name;
    bool bypassed = false;
};

struct TrackState
{
    int index = 0;
    std::string id;
    std::string name;
    int slot = -1;
    bool selected = false;

    PatchInstrumentState instrument;

    int volumeStep = trackVolumeDefaultStep;
    double volumeDb = 0.0;
    double gain = 1.0;

    std::vector<TrackHotFxState> hotFx;
};

struct TrackConfig
{
    std::string id;
    std::string name;
    int slot = -1;
    std::vector<std::string> hotFx;
    int volumeStep = trackVolumeDefaultStep;
};

double trackGainForStep (int step) noexcept;
double trackDbForStep (int step) noexcept;
int trackStepForGain (double gain) noexcept;

}
