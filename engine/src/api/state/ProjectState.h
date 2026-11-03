#pragma once

#include <functional>
#include <string>

namespace Api
{

struct ProjectSaveState
{
    std::string path;
    std::string name;

    bool dirty = false;
    bool saved = false;

    std::string error;
};

using ProjectStateProvider = std::function<ProjectSaveState()>;

}
