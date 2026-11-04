#pragma once

#include "SessionConfig.h"

#include <juce_core/juce_core.h>

#include <string>

namespace TrackGridE2E
{

bool makeTrackProject (const juce::File& tracksYaml,
                       const juce::File& projectFile,
                       std::string& errorText);

bool loadTrackConfig (const juce::File& projectFile,
                      const juce::String& pluginPath,
                      ihub::SessionConfig& config,
                      std::string& errorText);

}
