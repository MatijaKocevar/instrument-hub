#include "DeviceManager.h"

namespace AudioEngine
{

DeviceManager::DeviceManager() = default;

DeviceManager::~DeviceManager()
{
    if (audioCallback != nullptr)
        deviceManager.removeAudioCallback (audioCallback);

    jackQuantum.restore();
}

void DeviceManager::ensureInitialised()
{
    if (initialised)
        return;

    deviceManager.initialise (0, 0, nullptr, false);
    initialised = true;
}

void DeviceManager::setDeviceListOptions (const DeviceListOptions& options)
{
    listOptions = options;
    cardCache.clear();
    cardCacheValid = false;
}

const DeviceListOptions& DeviceManager::getDeviceListOptions() const noexcept
{
    return listOptions;
}

void DeviceManager::setShowTestDevices (bool show)
{
    if (listOptions.showTestDevices == show)
        return;

    listOptions.showTestDevices = show;
    cardCache.clear();
    cardCacheValid = false;
}

void DeviceManager::setRawDeviceList (bool raw)
{
    if (listOptions.raw == raw)
        return;

    listOptions.raw = raw;
    cardCache.clear();
    cardCacheValid = false;
}

void DeviceManager::close()
{
    deviceManager.closeAudioDevice();
    jackQuantum.restore();
    currentDisplayName.clear();
    cardCacheValid = false;
}

bool DeviceManager::isOpen() const
{
    return deviceManager.getCurrentAudioDevice() != nullptr;
}

void DeviceManager::setAudioCallback (juce::AudioIODeviceCallback* callback)
{
    if (callback == audioCallback)
        return;

    if (audioCallback != nullptr)
        deviceManager.removeAudioCallback (audioCallback);

    audioCallback = callback;

    if (audioCallback != nullptr)
        deviceManager.addAudioCallback (audioCallback);
}

juce::AudioIODeviceCallback* DeviceManager::getAudioCallback() const noexcept
{
    return audioCallback;
}

juce::AudioDeviceManager& DeviceManager::getNativeDeviceManager() noexcept
{
    return deviceManager;
}

const juce::AudioDeviceManager& DeviceManager::getNativeDeviceManager() const noexcept
{
    return deviceManager;
}

}
