#include "DeviceManager.h"

namespace AudioEngine
{

//==============================================================================
// Lifetime and callback plumbing. Device enumeration lives in
// device/DeviceEnumeration.cpp, opening/hot swapping in device/DeviceOpen.cpp,
// and the per-concern accessors next to them.

DeviceManager::DeviceManager() = default;

DeviceManager::~DeviceManager()
{
    if (audioCallback != nullptr)
        deviceManager.removeAudioCallback (audioCallback);
}

void DeviceManager::ensureInitialised()
{
    if (initialised)
        return;

    // Zero channels means no device is selected here; the wrapper opens one
    // only when open() is called, so constructing a manager never takes over
    // the system default output.
    deviceManager.initialise (0, 0, nullptr, false);
    initialised = true;
}

void DeviceManager::close()
{
    deviceManager.closeAudioDevice();
}

bool DeviceManager::isOpen() const
{
    return deviceManager.getCurrentAudioDevice() != nullptr;
}

//==============================================================================
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

} // namespace AudioEngine
