#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace Api
{

struct PluginState
{
    std::string id;
    std::string name;
    std::string description;
    std::string vendor;
    std::string version;
    std::string category;

    /** True when the plug-in identifies itself as an instrument. */
    bool isInstrument = false;

    /** True for an effect (the complement of `isInstrument`). */
    bool isEffect = true;

    std::string format;

    /** Bundle path (`.../Name.vst3`). */
    std::string path;
};

/** Everything `GET /plugins` returns. */
struct PluginInventoryState
{
    std::vector<PluginState> plugins;
    int total = 0;
    std::string query;
    bool scanning = false;
    bool scanned = false;
    std::int64_t lastScanMs = 0;
    std::string error;
};

/** Supplies a filtered inventory snapshot. Called on an HTTP thread. */
using PluginInventoryProvider =
    std::function<PluginInventoryState (const std::string& search)>;

/** Starts a background rescan. Returns false when one is already running. */
using PluginRescanHandler = std::function<bool()>;

} // namespace Api
