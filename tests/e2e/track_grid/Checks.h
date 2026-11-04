#pragma once

#include "track_grid/Result.h"
#include "track_grid/Rig.h"

namespace TrackGridE2E
{

void runPadSwitchPass (Rig& rig, TrackGridResult& result);

void runApiSwitchPass (Rig& rig, TrackGridResult& result);

void runTrackActions (Rig& rig, TrackGridResult& result);

void checkTrackLeds (Rig& rig, TrackGridResult& result);

}
