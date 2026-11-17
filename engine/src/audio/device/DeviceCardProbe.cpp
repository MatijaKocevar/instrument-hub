#include "audio/DeviceManager.h"

#include "audio/device/DeviceCards.h"

namespace AudioEngine {

namespace {
void fillChannelNames(DeviceInfo& info, juce::StringArray& names,
                      int fallbackCount) {
  if (names.isEmpty() && fallbackCount > 0)
    for (int index = 0; index < fallbackCount; ++index)
      names.add("channel " + juce::String(index + 1));

  if (!names.isEmpty()) info.outputChannelMask.setRange(0, names.size(), true);
}

void fillDeviceInfo(DeviceInfo& info, const juce::String& friendlyName,
                    const juce::String& juceName, const juce::String& pcmName,
                    bool hasInput, juce::AudioIODevice& device) {
  info.typeName = "ALSA";
  info.name = friendlyName;
  info.juceName = juceName;
  info.pcmName = pcmName;
  info.hasOutput = true;
  info.hasInput = hasInput;
  info.available = true;

  info.outputChannelNames = device.getOutputChannelNames();

  if (info.outputChannelNames.isEmpty())
    fillChannelNames(info, info.outputChannelNames,
                     detail::alsaPlaybackChannelCount(pcmName));

  info.inputChannelNames = device.getInputChannelNames();
  info.inputChannelMask.setRange(0, info.inputChannelNames.size(), true);
  info.outputChannelMask.setRange(0, info.outputChannelNames.size(), true);

  info.sampleRates = device.getAvailableSampleRates();
  info.bufferSizes = device.getAvailableBufferSizes();
}
}  // namespace

bool DeviceManager::probeListedDevice(const juce::String& friendlyName,
                                      const juce::String& juceName,
                                      const juce::String& pcmName,
                                      bool hasInput, DeviceInfo& info,
                                      bool& busy) {
  if (auto* current = deviceManager.getCurrentAudioDevice();
      current != nullptr && current->getName() == juceName) {
    fillDeviceInfo(info, friendlyName, juceName, pcmName, hasInput, *current);
    return true;
  }

  auto device =
      createRawDevice("ALSA", juceName, hasInput ? juceName : juce::String());

  if (device == nullptr) return false;

  const auto channelNames = device->getOutputChannelNames();
  const int channels =
      channelNames.isEmpty() ? 2 : juce::jmin(2, channelNames.size());

  juce::BigInteger outputMask;
  outputMask.setRange(0, channels, true);

  const auto error = device->open(juce::BigInteger(), outputMask, 48000.0, 128);

  if (error.isNotEmpty() || !device->isOpen()) {
    const int pcmError = detail::alsaPcmOpenError(pcmName);

    busy = pcmError == -EBUSY || error.containsIgnoreCase("busy") ||
           error.containsIgnoreCase("in use");
    return false;
  }

  fillDeviceInfo(info, friendlyName, juceName, pcmName, hasInput, *device);
  device->close();
  return true;
}

}  // namespace AudioEngine
