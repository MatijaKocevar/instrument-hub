#pragma once

#include "PatchState.h"

#include <string>
#include <vector>

namespace Api
{

inline constexpr int trackVolumeMinimumStep = 0;
inline constexpr int trackVolumeMaximumStep = 4;
inline constexpr int trackVolumeDefaultStep = 3;
inline constexpr double trackVolumeMinimumGain = 0.0;
inline constexpr double trackVolumeMaximumGain = 1.4125375446227544;

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
    double gain = 1.0;
};

double trackGainForStep (int step) noexcept;
double trackDbForGain (double gain) noexcept;
int trackVolumeStepForGain (double gain) noexcept;

}
