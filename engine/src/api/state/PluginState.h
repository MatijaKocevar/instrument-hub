#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace Api {

struct PluginState {
    std::string id;
    std::string name;
    std::string description;
    std::string vendor;
    std::string version;
    std::string category;

    bool isInstrument = false;

    bool isEffect = true;

    std::string format;

    std::string path;
};

struct PluginInventoryState {
    std::vector<PluginState> plugins;
    int total = 0;
    std::string query;
    bool scanning = false;
    bool scanned = false;
    std::int64_t lastScanMs = 0;
    std::string error;
};

using PluginInventoryProvider = std::function<PluginInventoryState(const std::string& search)>;

using PluginRescanHandler = std::function<bool()>;

}  // namespace Api
