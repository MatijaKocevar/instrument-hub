#pragma once

#include "../PluginInventory.h"

#include <vector>

namespace ihub::plugins::inventory_detail
{
std::vector<PluginInfo> toPluginInfos (const std::vector<juce::PluginDescription>& descriptions);

std::vector<PluginInfo> pluginsFromReport (const juce::File& dataDirectory);

}
