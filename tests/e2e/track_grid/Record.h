#pragma once

#include "track_grid/Leds.h"
#include "track_grid/Result.h"

#include <vector>

namespace TrackGridE2E
{

void writeTrackGridRecord (const TrackGridResult& result,
                           const std::vector<LedEvent>& leds);

}
