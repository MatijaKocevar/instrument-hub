#include "ProjectPaths.h"

#include "app/CommandLine.h"

namespace ihub::app {

juce::File defaultProjectFile() {
  return juce::File::getSpecialLocation(juce::File::userHomeDirectory)
      .getChildFile("Music")
      .getChildFile("Instrument Hub")
      .getChildFile("Untitled.ihub");
}

juce::File expandProjectPath(const juce::String& value) {
  if (value.isEmpty()) return {};

  if (value == "~")
    return juce::File::getSpecialLocation(juce::File::userHomeDirectory);

  if (value.startsWith("~/"))
    return juce::File::getSpecialLocation(juce::File::userHomeDirectory)
        .getChildFile(value.substring(2));

  if (juce::File::isAbsolutePath(value)) return juce::File(value);

  return juce::File::getCurrentWorkingDirectory().getChildFile(value);
}

ProjectPathResolution resolveProjectPath(
    const juce::String& commandLine,
    const ihub::settings::ProjectSettings& stored) {
  ProjectPathResolution result;

  if (const auto value = getCommandLineValue(commandLine, "--project");
      value.isNotEmpty()) {
    result.file = expandProjectPath(value);
    result.fromCommandLine = true;
    return result;
  }

  if (stored.lastPath.isNotEmpty()) {
    result.file = expandProjectPath(stored.lastPath);
    return result;
  }

  result.file = defaultProjectFile();
  return result;
}

juce::File resolveProjectFile(const juce::String& commandLine) {
  return resolveProjectPath(commandLine, {}).file;
}

}  // namespace ihub::app
