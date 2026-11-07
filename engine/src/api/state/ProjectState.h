#pragma once

#include <cstdint>
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

struct ProjectDialogRequest
{
    std::string mode;
    std::string currentPath;
    std::string suggestedName;
};

struct ProjectDialogStatus
{
    std::uint64_t id = 0;
    std::string state;
    std::string path;
};

using ProjectStateProvider = std::function<ProjectSaveState()>;
using ProjectOverviewProvider = std::function<ProjectOverview()>;
using ProjectCommandHandler =
    std::function<bool (const ProjectCommand& command, std::string& error)>;
using ProjectDialogBeginHandler =
    std::function<bool (const ProjectDialogRequest& request,
                        std::uint64_t& id,
                        std::string& error)>;
using ProjectDialogPollHandler =
    std::function<bool (std::uint64_t id,
                        ProjectDialogStatus& status,
                        std::string& error)>;

}
