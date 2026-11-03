#pragma once

#include <functional>
#include <string>
#include <vector>

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

struct RecentProjectState
{
    std::string path;
    std::string name;
};

struct ProjectOverview
{
    ProjectSaveState current;
    std::vector<RecentProjectState> recent;
};

struct ProjectCommand
{
    std::string action;
    std::string path;
    std::string name;
};

using ProjectStateProvider = std::function<ProjectSaveState()>;
using ProjectOverviewProvider = std::function<ProjectOverview()>;
using ProjectCommandHandler =
    std::function<bool (const ProjectCommand& command, std::string& error)>;

}
