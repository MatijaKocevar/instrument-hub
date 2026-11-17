#include "DeviceController.h"

#include "Session.h"

#include <algorithm>

namespace ihub::session {

bool DeviceController::resolve(const SessionDeviceConfig& config,
                               AudioEngine::DeviceSetup& setup,
                               juce::String& error) {
  const auto infos = devices.getDevices();

  const AudioEngine::DeviceInfo* chosen = nullptr;

  for (const auto& info : infos) {
    if (!info.hasOutput || info.outputChannelNames.isEmpty()) continue;

    if (config.preferLoopback && !info.name.containsIgnoreCase("Loopback")) {
      continue;
    }

    if (config.deviceTypeName.isNotEmpty() &&
        info.typeName != config.deviceTypeName) {
      continue;
    }

    if (config.outputDeviceName.isNotEmpty() &&
        !info.name.containsIgnoreCase(config.outputDeviceName) &&
        !info.juceName.containsIgnoreCase(config.outputDeviceName) &&
        !info.pcmName.containsIgnoreCase(config.outputDeviceName)) {
      continue;
    }

    chosen = &info;
    break;
  }

  if (chosen == nullptr) {
    if (config.preferLoopback)
      error = "no ALSA Loopback output device was enumerated";
    else if (config.outputDeviceName.isNotEmpty())
      error = "output device \"" + config.outputDeviceName + "\" was not found";
    else
      error = "no audio output device was enumerated";

    return false;
  }

  setup.deviceTypeName = chosen->typeName;
  setup.outputDeviceName = chosen->name;

  const int availableOutputs = chosen->outputChannelNames.size();
  const int requestedOutputs = std::max(1, config.numOutputChannels);
  setup.numOutputChannels = juce::jlimit(1, availableOutputs, requestedOutputs);
  setup.outputChannels =
      AudioEngine::DeviceSetup::channelMask(setup.numOutputChannels);

  setup.inputDeviceName = {};
  setup.numInputChannels = 0;
  setup.inputChannels.clear();

  if (chosen->hasInput && !chosen->inputChannelNames.isEmpty() &&
      config.numInputChannels > 0) {
    setup.inputDeviceName = config.inputDeviceName.isNotEmpty()
                                ? config.inputDeviceName
                                : chosen->name;

    const int availableInputs = chosen->inputChannelNames.size();
    setup.numInputChannels =
        juce::jlimit(1, availableInputs, config.numInputChannels);
    setup.inputChannels =
        AudioEngine::DeviceSetup::channelMask(setup.numInputChannels);
  }

  setup.sampleRate = config.sampleRate > 0.0 ? config.sampleRate : 48000.0;
  setup.bufferSize = config.bufferSize > 0 ? config.bufferSize : 128;

  return true;
}

bool DeviceController::open(const AudioEngine::DeviceSetup& setup,
                            juce::String& error) {
  return devices.open(setup, &error);
}

void DeviceController::close() {
  if (callbackInstalled) {
    devices.setAudioCallback(nullptr);
    callbackInstalled = false;
  }

  if (devices.isOpen()) devices.close();
}

void DeviceController::setCallback(juce::AudioIODeviceCallback* callback) {
  devices.setAudioCallback(callback);
  callbackInstalled = callback != nullptr;
}

}  // namespace ihub::session
