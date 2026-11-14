#include "PluginBridge.h"

#include "plugins/PluginInventory.h"

#include <utility>

namespace Api
{
    namespace
    {
        PluginInventoryState toApiInventory (ihub::plugins::PluginInventorySnapshot snapshot)
        {
            PluginInventoryState result;
            result.total = snapshot.total;
            result.query = std::move (snapshot.query);
            result.scanning = snapshot.scanning;
            result.scanned = snapshot.scanned;
            result.lastScanMs = snapshot.lastScanMs;
            result.error = std::move (snapshot.error);
            result.plugins.reserve (snapshot.plugins.size ());

            for (const auto& plugin : snapshot.plugins)
            {
                PluginState state;
                state.id = plugin.id;
                state.name = plugin.name;
                state.description = plugin.description;
                state.vendor = plugin.vendor;
                state.version = plugin.version;
                state.category = plugin.category;
                state.isInstrument = plugin.isInstrument;
                state.isEffect = plugin.isEffect;
                state.format = plugin.format;
                state.path = plugin.path;
                result.plugins.push_back (std::move (state));
            }

            return result;
        }
    }

    void attachPluginInventory (Server& server, ihub::plugins::PluginInventory& inventory)
    {
        server.setPluginHandlers (
            [&inventory] (const std::string& search)
            { return toApiInventory (inventory.snapshot (juce::String (search))); },
            [&inventory] { return inventory.requestRescan (); });
    }

}
