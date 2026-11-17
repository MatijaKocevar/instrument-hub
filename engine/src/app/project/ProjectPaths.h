#pragma once

#include "settings/SettingsStore.h"

#include <juce_core/juce_core.h>

namespace ihub::app {

struct ProjectPathResolution {
    juce::File file;
    bool fromCommandLine = false;
};

juce::File defaultProjectFile();

juce::File expandProjectPath(const juce::String& value);

ProjectPathResolution resolveProjectPath(const juce::String& commandLine,
                                         const ihub::settings::ProjectSettings& stored);

juce::File resolveProjectFile(const juce::String& commandLine);

}  // namespace ihub::app
