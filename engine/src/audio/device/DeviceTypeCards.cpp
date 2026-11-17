#include "audio/DeviceManager.h"

#include <utility>

namespace AudioEngine {
namespace {
juce::String typeDescription(const juce::String& typeName) {
  if (typeName == "JACK") return "JACK audio device";

  return typeName + " audio device";
}

void fillChannelInfo(DeviceInfo& info, juce::AudioIODevice& device) {
  info.inputChannelNames = device.getInputChannelNames();
  info.outputChannelNames = device.getOutputChannelNames();
  info.inputChannelMask.setRange(0, info.inputChannelNames.size(), true);
  info.outputChannelMask.setRange(0, info.outputChannelNames.size(), true);
  info.sampleRates = device.getAvailableSampleRates();
  info.bufferSizes = device.getAvailableBufferSizes();
}
}  // namespace

std::vector<DeviceInfo> DeviceManager::getTypeDevices() {
  std::vector<DeviceInfo> result;

  for (auto* type : deviceManager.getAvailableDeviceTypes()) {
    type->scanForDevices();

    const auto typeName = type->getTypeName();

    if (typeName == "ALSA") continue;

    const auto inputNames = type->getDeviceNames(true);
    const auto outputNames = type->getDeviceNames(false);

    juce::StringArray allNames(inputNames);

    for (const auto& name : outputNames) allNames.addIfNotAlreadyThere(name);

    for (const auto& name : allNames) {
      DeviceInfo info;
      info.typeName = typeName;
      info.name = name;
      info.juceName = name;
      info.description = typeDescription(typeName);
      info.hasInput = inputNames.contains(name);
      info.hasOutput = outputNames.contains(name);
      info.available = true;

      auto device =
          createRawDevice(typeName, info.hasOutput ? name : juce::String(),
                          info.hasInput ? name : juce::String());

      if (device != nullptr) fillChannelInfo(info, *device);

      result.push_back(std::move(info));
    }
  }

  return result;
}

}  // namespace AudioEngine
