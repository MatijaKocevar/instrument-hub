#include "settings/ProjectHistory.h"

#include <algorithm>

namespace ihub::settings {

juce::String projectNameForPath(const juce::String& path) {
  if (path.isEmpty()) return {};

  return juce::File(path).getFileNameWithoutExtension();
}

void rememberRecentProject(ProjectSettings& project, const juce::String& path,
                           const juce::String& name) {
  if (path.isEmpty()) return;

  auto& recent = project.recent;
  recent.erase(std::remove_if(recent.begin(), recent.end(),
                              [&path](const RecentProject& entry) {
                                return entry.path == path;
                              }),
               recent.end());

  RecentProject entry;
  entry.path = path;
  entry.name = name.isNotEmpty() ? name : projectNameForPath(path);
  recent.insert(recent.begin(), std::move(entry));

  if (recent.size() > maximumRecentProjects)
    recent.resize(maximumRecentProjects);

  project.lastPath = path;
}

bool removeRecentProject(ProjectSettings& project, const juce::String& path) {
  const auto previous = project.recent.size();

  project.recent.erase(
      std::remove_if(
          project.recent.begin(), project.recent.end(),
          [&path](const RecentProject& entry) { return entry.path == path; }),
      project.recent.end());

  if (project.lastPath == path) project.lastPath.clear();

  return project.recent.size() != previous;
}

}  // namespace ihub::settings
