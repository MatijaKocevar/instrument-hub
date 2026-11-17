#pragma once

#include "project/SetFile.h"

#include <juce_core/juce_core.h>

#include <map>
#include <vector>

namespace ihub::app {

bool importSourceAssignments(const juce::File& sourcesFile,
                             std::map<int, ihub::project::SlotConfig>& slots, juce::String& error);

bool importPluginStates(const juce::File& pluginStateDirectory,
                        std::map<int, ihub::project::SlotConfig>& slots,
                        std::vector<ihub::project::PluginStateChunk>& chunks, juce::String& error);

}  // namespace ihub::app
