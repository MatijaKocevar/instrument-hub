#include "ProjectLifecycle.h"

#include "../TestSupport.h"

#include "app/project/ProjectPaths.h"
#include "settings/ProjectHistory.h"
#include "settings/SettingsStore.h"

#include <juce_core/juce_core.h>

#include <string>

namespace ApiTest::ProjectLifecycle
{

bool runHistory (const juce::File& root)
{
    const auto settingsFile = root.getChildFile ("history-settings.json");
    ihub::settings::SettingsStore settings (settingsFile);
    (void) settings.load();

    const auto first = root.getChildFile ("First.ihub");
    const auto second = root.getChildFile ("Second.ihub");

    ihub::settings::rememberRecentProject (settings.settings().project,
                                           first.getFullPathName(), "First");
    ihub::settings::rememberRecentProject (settings.settings().project,
                                           second.getFullPathName(), "Second");

    check (settings.save(), "the project history settings save: "
                                + settings.getLastError().toStdString());

    ihub::settings::SettingsStore reloaded (settingsFile);
    check (reloaded.load() == ihub::settings::LoadStatus::loaded,
           "the project history settings reload");

    auto stored = reloaded.settings().project;
    check (stored.lastPath == second.getFullPathName(),
           "the settings remember the last opened project");
    check (stored.recent.size() == 2
               && stored.recent[0].path == second.getFullPathName()
               && stored.recent[1].path == first.getFullPathName(),
           "the recent list survives a restart in newest-first order");

    ihub::settings::rememberRecentProject (stored, first.getFullPathName(), "First");
    check (stored.recent.size() == 2 && stored.recent[0].path == first.getFullPathName(),
           "reopening a project moves it to the front of the recent list");

    for (int index = 0; index < 20; ++index)
        ihub::settings::rememberRecentProject (
            stored,
            root.getChildFile ("Recent " + juce::String (index) + ".ihub")
                .getFullPathName(),
            "Recent " + juce::String (index));

    check (stored.recent.size() == ihub::settings::maximumRecentProjects,
           "the recent list is capped");

    const auto resolved = ihub::app::resolveProjectPath ("", stored);
    check (resolved.file == juce::File (stored.lastPath) && ! resolved.fromCommandLine,
           "startup resolves the stored last project");

    const auto overrideFile = root.getChildFile ("Override.ihub");
    const auto overridden = ihub::app::resolveProjectPath (
        "--project " + overrideFile.getFullPathName(), stored);
    check (overridden.file == overrideFile && overridden.fromCommandLine,
           "--project overrides the stored path for this run");

    const auto fromEnvironment = ihub::app::resolveProjectPath ("", {});
    check (fromEnvironment.file.getFileName() == "Untitled.ihub"
               && ! fromEnvironment.fromCommandLine,
           "a missing history falls back to the default project");

    return true;
}

}
