#include "AudioSettings.h"

#include <utility>

namespace AudioEngine
{

AudioSettingsBridge::AudioSettingsBridge()
{
    refreshDevices();

    if (devices.empty())
        return;

    pendingType = firstDeviceType();
    pendingName = firstOutputDeviceOfType (pendingType);
}

void AudioSettingsBridge::setDeviceManager (DeviceManager* manager)
{
    const std::lock_guard<std::mutex> lock (mutex);

    injectedManager = manager;
    refreshDevices();
    if (devices.empty())
        return;

    if (pendingName.isEmpty())
    {
        pendingType = firstDeviceType();
        pendingName = firstOutputDeviceOfType (pendingType);
    }
}

void AudioSettingsBridge::setSettingsStore (ihub::settings::SettingsStore* store)
{
    const std::lock_guard<std::mutex> lock (mutex);
    settingsStore = store;
}

void AudioSettingsBridge::setRestoreState (const Api::AudioRestoreState& state)
{
    const std::lock_guard<std::mutex> lock (mutex);
    restoreState = state;
}

void AudioSettingsBridge::refreshDevices()
{
    deviceTypeNames = liveManager().getDeviceTypeNames();
    devices.clear();

    for (const auto& info : liveManager().getDevices())
    {
        CachedDevice cached;
        cached.type = info.typeName;
        cached.name = info.name;
        cached.hasInput = info.hasInput;
        cached.hasOutput = info.hasOutput;
        cached.inputNames = info.inputChannelNames;
        cached.outputNames = info.outputChannelNames;
        cached.sampleRates = info.sampleRates;
        cached.bufferSizes = info.bufferSizes;
        devices.push_back (std::move (cached));
    }
}

const AudioSettingsBridge::CachedDevice* AudioSettingsBridge::findDevice (
    const juce::String& type,
    const juce::String& name) const
{
    for (const auto& device : devices)
        if (device.type == type && device.name == name)
            return &device;

    return nullptr;
}

juce::String AudioSettingsBridge::firstDeviceType() const
{
    for (const auto& device : devices)
        if (device.type == "ALSA")
            return device.type;

    return devices.empty() ? juce::String() : devices.front().type;
}

juce::String AudioSettingsBridge::firstOutputDeviceOfType (const juce::String& type) const
{
    for (const auto& device : devices)
        if (device.type == type && device.hasOutput)
            return device.name;

    for (const auto& device : devices)
        if (device.hasOutput)
            return device.name;

    return {};
}

void AudioSettingsBridge::setChainLatencySamples (int samples) noexcept
{
    chainLatencySamples.store (juce::jmax (0, samples), std::memory_order_relaxed);
}

void AudioSettingsBridge::setTimingMonitor (TimingMonitor* monitor) noexcept
{
    timingMonitor = monitor;
}

}
