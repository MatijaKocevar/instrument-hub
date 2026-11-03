#include "audio/AudioSettings.h"

#include "audio/device/DeviceErrors.h"

namespace AudioEngine
{

bool AudioSettingsBridge::openCachedDevice (const CachedDevice& device,
                                            juce::String* errorMessage)
{
    if (! device.available)
    {
        if (errorMessage != nullptr)
        {
            const auto reason = detail::describeDeviceError (device.unavailableReason);
            *errorMessage = reason.isNotEmpty() ? reason : "the device is not connected";
        }

        return false;
    }

    DeviceSetup setup;
    setup.deviceTypeName = device.type;
    setup.outputDeviceName = device.name;
    setup.inputDeviceName = device.hasInput ? device.name : juce::String();
    setup.sampleRate = 48000.0;
    setup.bufferSize = 128;
    setup.numOutputChannels = device.outputNames.isEmpty()
                                  ? 2
                                  : juce::jmin (2, device.outputNames.size());
    setup.numInputChannels = device.hasInput ? device.inputNames.size() : 0;
    setup.outputChannels = DeviceSetup::channelMask (setup.numOutputChannels);
    setup.inputChannels = DeviceSetup::channelMask (setup.numInputChannels);

    auto& manager = liveManager();

    if (! manager.open (setup, errorMessage))
        return false;

    pendingType = setup.deviceTypeName;
    pendingName = setup.outputDeviceName;
    pendingSampleRate = manager.getCurrentSampleRate();
    pendingBufferSize = manager.getCurrentBufferSize();
    return true;
}

bool AudioSettingsBridge::openLoopback (juce::String* errorMessage)
{
    const std::lock_guard<std::mutex> lock (mutex);

    if (devices.empty())
        refreshDevices();

    const CachedDevice* loopback = nullptr;

    for (const auto& device : devices)
        if (device.hasOutput && device.name.containsIgnoreCase ("Loopback"))
        {
            loopback = &device;
            break;
        }

    if (loopback == nullptr)
    {
        if (errorMessage != nullptr)
            *errorMessage = "no Loopback output device was enumerated";

        return false;
    }

    return openCachedDevice (*loopback, errorMessage);
}

bool AudioSettingsBridge::openFirstOutput (juce::String* errorMessage)
{
    const std::lock_guard<std::mutex> lock (mutex);

    if (devices.empty())
        refreshDevices();

    const CachedDevice* chosen = nullptr;

    for (const auto& device : devices)
        if (device.hasOutput && device.available)
        {
            chosen = &device;
            break;
        }

    if (chosen == nullptr)
    {
        if (errorMessage != nullptr)
            *errorMessage = "no output device was enumerated";

        return false;
    }

    return openCachedDevice (*chosen, errorMessage);
}

void AudioSettingsBridge::close()
{
    const std::lock_guard<std::mutex> lock (mutex);
    liveManager().close();
}

}
