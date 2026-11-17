#pragma once

#include <functional>
#include <string>
#include <vector>

namespace Api {

struct PluginPageStatus {
    std::string id = "tracks";
    std::string status = "ready";
    std::vector<int> slots;
    int activeSlot = -1;
    std::string error;
};

using PluginPageProvider = std::function<std::vector<PluginPageStatus>()>;

}  // namespace Api
