#pragma once

#include "audio/DeviceManager.h"
#include "audio/settings/AudioDevicePreference.h"

#include <vector>

namespace AudioEngine::detail {

const DeviceInfo* findOutputDevice(const std::vector<DeviceInfo>& devices, const juce::String& type,
                                   const juce::String& name, bool exactName);

const DeviceInfo* firstOutputDeviceOfType(const std::vector<DeviceInfo>& devices,
                                          const juce::String& type);

const DeviceInfo* defaultOutputDevice(const std::vector<DeviceInfo>& devices);

AudioDevicePreference devicePreference(const DeviceInfo& device, double sampleRate, int bufferSize);

bool openFallbackDevice(const std::vector<DeviceInfo>& devices, const DeviceInfo* skip,
                        double sampleRate, int bufferSize, const AudioDeviceOpener& opener,
                        AudioDevicePreference& preference, juce::String& error);

}  // namespace AudioEngine::detail
