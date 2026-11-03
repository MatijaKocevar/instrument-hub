#pragma once

#include "api/Server.h"
#include "api/StateStore.h"
#include "api/state/ProjectState.h"

#include <juce_core/juce_core.h>

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace ihub
{
class Session;
struct SessionConfig;
}

namespace ihub::project
{
struct SetFile;
}

namespace ihub::settings
{
class SettingsStore;
}

namespace ihub::app
{

class ProjectAutosave;

class ProjectLifecycle
{
public:
    ProjectLifecycle (ihub::Session& session,
                      Api::StateStore& state,
                      Api::Server& server,
                      ihub::settings::SettingsStore* settings);
    ~ProjectLifecycle();

    ProjectLifecycle (const ProjectLifecycle&) = delete;
    ProjectLifecycle& operator= (const ProjectLifecycle&) = delete;

    bool attach (const juce::File& file, bool remember, juce::String& error);

    bool apply (const Api::ProjectCommand& command, std::string& error);

    Api::ProjectSaveState getState() const;
    Api::ProjectOverview getOverview() const;

    void noteEdit (const char* kind, int slot);
    void flush();
    void shutdown();

    using OpenedHandler = std::function<void (const juce::File&)>;
    void setOpenedHandler (OpenedHandler handler);

private:
    bool openProject (const juce::File& file, bool remember, std::string& error);
    bool newProject (const juce::String& path, const juce::String& name,
                     std::string& error);
    bool saveAs (const juce::File& file, std::string& error);
    bool saveCurrent (std::string& error);
    bool closeProject (std::string& error);

    bool rebuildSession (const ihub::SessionConfig& config, juce::String& error);
    void installAutosave (const juce::File& file,
                          std::shared_ptr<ihub::project::SetFile> base);
    void recordProject (const juce::File& file, bool remember);
    void notifyChanged();

    std::shared_ptr<ProjectAutosave> currentAutosave() const;
    Api::ProjectSaveState currentErrorState() const;

    juce::File uniqueUntitledFile() const;

    ihub::Session& session;
    Api::StateStore& state;
    Api::Server& server;
    ihub::settings::SettingsStore* settingsStore;

    mutable std::mutex mutex;
    mutable std::mutex stateMutex;
    mutable std::mutex historyMutex;

    std::shared_ptr<ProjectAutosave> autosave;
    std::shared_ptr<ihub::project::SetFile> base;
    Api::ProjectSaveState errorState;
    OpenedHandler openedHandler;
    bool closed = false;
};

}
