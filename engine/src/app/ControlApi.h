#pragma once

#include "api/state/AudioState.h"

#include <juce_core/juce_core.h>

#include <functional>
#include <memory>

namespace Api {
class MidiBridge;
class Server;
class StateStore;
}  // namespace Api

namespace AudioEngine {
class AudioSettingsBridge;
}

namespace ihub {
class Session;
}

namespace ihub::plugins {
class PluginInventory;
}

namespace ihub::settings {
class SettingsStore;
}

namespace ihub::app {

class ProjectDialog;
class ProjectLifecycle;

class MidiDeviceWatcher;

class ControlApi {
 public:
  ControlApi();
  ~ControlApi();

  bool start(const juce::String& commandLine, ihub::Session* session,
             bool required, const juce::File& projectFile,
             std::function<void()> midiRestore = {},
             ihub::settings::SettingsStore* settings = nullptr,
             const Api::AudioRestoreState& audioRestore = {},
             bool persistProjectPath = true,
             std::function<void(const juce::File&)> projectOpened = {});

  void stopServer();
  void detachMidi();
  void resetServer();
  void resetAudio();
  void flushProject();

  Api::StateStore* getState() noexcept { return apiState.get(); }
  Api::MidiBridge* getMidiBridge() noexcept { return midiBridge.get(); }

 private:
  void resetProject();

  std::unique_ptr<ihub::plugins::PluginInventory> pluginInventory;
  std::unique_ptr<Api::StateStore> apiState;
  std::unique_ptr<Api::Server> apiServer;
  std::unique_ptr<Api::MidiBridge> midiBridge;
  std::unique_ptr<MidiDeviceWatcher> midiDeviceWatcher;
  std::unique_ptr<AudioEngine::AudioSettingsBridge> audioSettings;
  std::unique_ptr<ProjectDialog> projectDialog;
  std::unique_ptr<ProjectLifecycle> projectLifecycle;
};

}  // namespace ihub::app
