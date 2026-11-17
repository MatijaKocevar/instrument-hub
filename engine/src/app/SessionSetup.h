#pragma once

#include "api/state/AudioState.h"

#include <juce_core/juce_core.h>

#include <memory>

namespace ihub {
class Session;
}

namespace ihub::settings {
class SettingsStore;
}

namespace ihub::app {

struct SessionStartup {
  std::unique_ptr<ihub::Session> session;
  Api::AudioRestoreState audioRestore;
};

SessionStartup startSession(const juce::String& commandLine,
                            const juce::File& projectFile,
                            ihub::settings::SettingsStore* settings = nullptr);

}  // namespace ihub::app
