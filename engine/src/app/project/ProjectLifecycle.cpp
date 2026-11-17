#include "ProjectLifecycle.h"

#include "ProjectAutosave.h"
#include "ProjectCapture.h"
#include "ProjectPaths.h"

#include "project/Serializer.h"
#include "settings/ProjectHistory.h"
#include "settings/SettingsStore.h"

#include <utility>

namespace ihub::app {

namespace {

Api::ProjectSaveState errorProjectState(const juce::File& file,
                                        const juce::String& message) {
  Api::ProjectSaveState state;
  state.path = file.getFullPathName().toStdString();
  state.name = file.getFileNameWithoutExtension().toStdString();
  state.saved = false;
  state.error = message.toStdString();
  return state;
}

}  // namespace

ProjectLifecycle::ProjectLifecycle(ihub::Session& sessionToUse,
                                   Api::StateStore& stateToUse,
                                   Api::Server& serverToUse,
                                   ihub::settings::SettingsStore* settings)
    : session(sessionToUse),
      state(stateToUse),
      server(serverToUse),
      settingsStore(settings) {
  state.setEditHandler(
      [this](const char* kind, int slot) { noteEdit(kind, slot); });

  state.setProjectStateProvider([this] { return getState(); });

  server.setProjectHandlers(
      [this] { return getOverview(); },
      [this](const Api::ProjectCommand& command, std::string& error) {
        return apply(command, error);
      });
}

ProjectLifecycle::~ProjectLifecycle() {
  shutdown();
  state.setEditHandler({});
  state.setProjectStateProvider({});
  server.setProjectHandlers({}, {});
}

bool ProjectLifecycle::attach(const juce::File& file, bool remember,
                              juce::String& error) {
  const std::lock_guard<std::mutex> lock(mutex);

  {
    const std::lock_guard<std::mutex> stateLock(stateMutex);
    auto previous = std::move(autosave);
    autosave.reset();
    base.reset();
    closed = false;
    errorState = {};

    if (previous != nullptr) previous->shutdown();
  }

  if (file == juce::File()) {
    error = "no project file is available";

    {
      const std::lock_guard<std::mutex> stateLock(stateMutex);
      errorState = errorProjectState(file, error);
    }

    notifyChanged();
    return false;
  }

  auto set = std::make_shared<project::SetFile>();

  if (!project::Serializer::load(file, *set, error)) {
    {
      const std::lock_guard<std::mutex> stateLock(stateMutex);
      errorState = errorProjectState(file, error);
    }

    notifyChanged();
    return false;
  }

  installAutosave(file, std::move(set));
  recordProject(file, remember);
  notifyChanged();
  error.clear();
  return true;
}

bool ProjectLifecycle::apply(const Api::ProjectCommand& command,
                             std::string& error) {
  const std::lock_guard<std::mutex> lock(mutex);

  if (command.action == "new")
    return newProject(juce::String(command.path), juce::String(command.name),
                      error);

  if (command.action == "open") {
    if (command.path.empty()) {
      error = "open needs a project path";
      return false;
    }

    return openProject(expandProjectPath(juce::String(command.path)), true,
                       error);
  }

  if (command.action == "save") {
    if (saveCurrent(error)) {
      notifyChanged();
      return true;
    }

    return false;
  }

  if (command.action == "save-as") {
    if (command.path.empty()) {
      error = "save-as needs a project path";
      return false;
    }

    return saveAs(expandProjectPath(juce::String(command.path)), error);
  }

  if (command.action == "close") return closeProject(error);

  error = "unknown project action \"" + command.action + "\"";
  return false;
}

Api::ProjectSaveState ProjectLifecycle::getState() const {
  if (const auto current = currentAutosave(); current != nullptr)
    return current->getState();

  return currentErrorState();
}

Api::ProjectOverview ProjectLifecycle::getOverview() const {
  Api::ProjectOverview overview;

  if (const auto current = currentAutosave(); current != nullptr)
    overview.current = current->getState();
  else
    overview.current = currentErrorState();

  if (settingsStore != nullptr) {
    const std::lock_guard<std::mutex> historyLock(historyMutex);

    for (const auto& entry : settingsStore->settings().project.recent) {
      if (!juce::File(entry.path).existsAsFile()) continue;

      overview.recent.push_back(
          {entry.path.toStdString(), entry.name.toStdString()});
    }
  }

  return overview;
}

void ProjectLifecycle::noteEdit(const char* kind, int slot) {
  const std::lock_guard<std::mutex> lock(mutex);

  bool isClosed = false;

  {
    const std::lock_guard<std::mutex> stateLock(stateMutex);
    isClosed = closed;
  }

  if (isClosed) return;

  if (const auto current = currentAutosave(); current != nullptr)
    current->noteEdit(kind, slot);
}

void ProjectLifecycle::flush() {
  const std::lock_guard<std::mutex> lock(mutex);

  if (const auto current = currentAutosave(); current != nullptr) {
    juce::String error;

    if (!current->saveNow(&error) && error.isNotEmpty())
      juce::Logger::writeToLog("project: could not save: " + error);
  }
}

void ProjectLifecycle::shutdown() {
  const std::lock_guard<std::mutex> lock(mutex);

  const auto current = currentAutosave();

  {
    const std::lock_guard<std::mutex> stateLock(stateMutex);
    autosave.reset();
    closed = true;
    base.reset();
  }

  if (current != nullptr) current->shutdown();
}

void ProjectLifecycle::setOpenedHandler(OpenedHandler handler) {
  const std::lock_guard<std::mutex> lock(mutex);
  openedHandler = std::move(handler);
}

void ProjectLifecycle::installAutosave(
    const juce::File& file, std::shared_ptr<project::SetFile> newBase) {
  auto replacement = std::make_shared<ProjectAutosave>(file);
  auto captureBase = newBase;

  replacement->setCaptureHandler(
      [this, captureBase](project::SetFile& set,
                          const std::vector<int>& touchedSlots,
                          juce::String& captureError) {
        return captureProject(session, state, *captureBase, touchedSlots, set,
                              captureError);
      });

  replacement->setStateChangedHandler([this] { notifyChanged(); });

  std::shared_ptr<ProjectAutosave> previous;

  {
    const std::lock_guard<std::mutex> stateLock(stateMutex);
    previous = autosave;
    autosave = replacement;
    base = std::move(newBase);
    closed = false;
    errorState = {};
  }

  if (previous != nullptr) previous->shutdown();
}

void ProjectLifecycle::recordProject(const juce::File& file, bool remember) {
  if (!remember || settingsStore == nullptr) return;

  const std::lock_guard<std::mutex> historyLock(historyMutex);

  auto& project = settingsStore->settings().project;
  const auto path = file.getFullPathName();

  settings::rememberRecentProject(project, path,
                                  settings::projectNameForPath(path));

  if (!settingsStore->save())
    juce::Logger::writeToLog("settings: could not remember the project: " +
                             settingsStore->getLastError());
}

void ProjectLifecycle::notifyChanged() { state.notifyProjectStateChanged(); }

std::shared_ptr<ProjectAutosave> ProjectLifecycle::currentAutosave() const {
  const std::lock_guard<std::mutex> stateLock(stateMutex);
  return autosave;
}

Api::ProjectSaveState ProjectLifecycle::currentErrorState() const {
  const std::lock_guard<std::mutex> stateLock(stateMutex);
  return errorState;
}

}  // namespace ihub::app
