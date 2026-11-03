#pragma once

#include "settings/SettingsStore.h"

namespace ihub::settings
{

constexpr std::size_t maximumRecentProjects = 8;

juce::String projectNameForPath (const juce::String& path);

void rememberRecentProject (ProjectSettings& project,
                            const juce::String& path,
                            const juce::String& name = {});

bool removeRecentProject (ProjectSettings& project, const juce::String& path);

}
