#pragma once

#include <string>
#include <vector>

namespace ihub::app
{

struct TrackHotFxUpdate
{
    std::string trackId;
    std::vector<std::string> hotFx;
};

bool rewriteTrackHotFx (const std::string& mappingYaml,
                        const std::vector<TrackHotFxUpdate>& updates,
                        std::string& destination);

}
