#pragma once

#include "../PluginInventory.h"

#include <vector>

namespace ihub::plugins::inventory_detail
{
std::vector<PluginInfo> toPluginInfos (const std::vector<juce::PluginDescription>& descriptions);

/** Legacy fallback: a KnownPluginList.xml without a cache entry.

    Only bundles that still exist are kept, so a removed bundle does not
    reappear just because it is still in the old report.
*/
std::vector<PluginInfo> pluginsFromReport (const juce::File& dataDirectory);

} // namespace ihub::plugins::inventory_detail
