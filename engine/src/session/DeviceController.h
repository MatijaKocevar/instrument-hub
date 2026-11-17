#pragma once

#include "audio/DeviceManager.h"

#include <juce_core/juce_core.h>

namespace ihub {
struct SessionDeviceConfig;
}

namespace ihub::session {

class DeviceController {
 public:
  bool resolve(const SessionDeviceConfig& config,
               AudioEngine::DeviceSetup& setup, juce::String& error);

  bool open(const AudioEngine::DeviceSetup& setup, juce::String& error);
  void close();

  bool isOpen() const noexcept { return devices.isOpen(); }

  void setCallback(juce::AudioIODeviceCallback* callback);
  bool hasCallback() const noexcept { return callbackInstalled; }

  AudioEngine::DeviceManager& getManager() noexcept { return devices; }
  const AudioEngine::DeviceManager& getManager() const noexcept {
    return devices;
  }

  double getCurrentSampleRate() const noexcept {
    return devices.getCurrentSampleRate();
  }
  int getCurrentBufferSize() const noexcept {
    return devices.getCurrentBufferSize();
  }
  int getActiveOutputChannels() const noexcept {
    return devices.getActiveOutputChannels().countNumberOfSetBits();
  }

 private:
  AudioEngine::DeviceManager devices;
  bool callbackInstalled = false;
};

}  // namespace ihub::session
