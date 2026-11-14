#pragma once

#include "api/state/TrackState.h"

#include <juce_core/juce_core.h>

#include <string>
#include <vector>

namespace ihub::app
{

    bool saveTrackLayoutToMapping (const std::string& mappingYaml,
                                   const std::vector<Api::TrackConfig>& tracks,
                                   std::string& destination,
                                   juce::String& error);

}
