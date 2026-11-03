#include "audio/DeviceManager.h"

#include "audio/device/DeviceErrors.h"

#include <cmath>

namespace AudioEngine
{

namespace
{
bool sameRate (double a, double b)
{
    return std::abs (a - b) < 0.5;
}
}

juce::AudioDeviceManager::AudioDeviceSetup DeviceManager::makeNativeSetup (const DeviceSetup& setup) const
{
    juce::AudioDeviceManager::AudioDeviceSetup native;
    native.inputDeviceName = setup.inputDeviceName;
    native.outputDeviceName = setup.outputDeviceName;
    native.sampleRate = setup.sampleRate;
    native.bufferSize = setup.bufferSize;
    native.useDefaultInputChannels = false;
    native.useDefaultOutputChannels = false;

    native.inputChannels = setup.inputChannels;

    if (native.inputChannels.isZero() && setup.inputDeviceName.isNotEmpty())
        native.inputChannels = DeviceSetup::channelMask (setup.numInputChannels);

    native.outputChannels = setup.outputChannels;

    if (native.outputChannels.isZero() && setup.outputDeviceName.isNotEmpty())
        native.outputChannels = DeviceSetup::channelMask (setup.numOutputChannels);

    return native;
}

bool DeviceManager::applyNativeSetup (const juce::AudioDeviceManager::AudioDeviceSetup& native,
                                      juce::String* errorMessage)
{
    const auto error = deviceManager.setAudioDeviceSetup (native, true);
    const bool open = deviceManager.getCurrentAudioDevice() != nullptr;

    if (errorMessage != nullptr)
    {
        if (error.isNotEmpty())
            *errorMessage = detail::describeDeviceError (error);
        else if (! open)
            *errorMessage = "no audio device was opened";
    }

    return error.isEmpty() && open;
}

bool DeviceManager::open (const DeviceSetup& setup, juce::String* errorMessage)
{
    ensureInitialised();

    if (setup.inputDeviceName.isEmpty() && setup.outputDeviceName.isEmpty())
    {
        if (errorMessage != nullptr)
            *errorMessage = "no input or output device name given";

        return false;
    }

    auto typeName = setup.deviceTypeName;

    if (typeName.isEmpty())
        typeName = deviceManager.getCurrentAudioDeviceType();

    if (findDeviceType (typeName) == nullptr)
    {
        const auto available = getDeviceTypeNames();

        if (available.isEmpty())
        {
            if (errorMessage != nullptr)
                *errorMessage = "no audio device types are available";

            return false;
        }

        typeName = available[0];
    }

    if (deviceManager.getCurrentAudioDeviceType() != typeName)
    {
        deviceManager.setCurrentAudioDeviceType (typeName, true);
    }

    auto resolved = setup;
    resolved.deviceTypeName = typeName;
    resolved.outputDeviceName = resolveListedName (typeName, setup.outputDeviceName);

    if (setup.inputDeviceName.isNotEmpty())
        resolved.inputDeviceName = setup.inputDeviceName == setup.outputDeviceName
                                       ? resolved.outputDeviceName
                                       : resolveListedName (typeName, setup.inputDeviceName);

    if (! applyNativeSetup (makeNativeSetup (resolved), errorMessage))
        return false;

    currentDisplayName = setup.outputDeviceName.isNotEmpty() ? setup.outputDeviceName
                                                            : setup.inputDeviceName;
    cardCacheValid = false;
    return true;
}

bool DeviceManager::setDevice (const DeviceSetup& setup, juce::String* errorMessage)
{
    return open (setup, errorMessage);
}

bool DeviceManager::setBufferSize (int bufferSize, juce::String* errorMessage)
{
    if (! isOpen())
    {
        if (errorMessage != nullptr)
            *errorMessage = "no audio device is open";

        return false;
    }

    auto native = deviceManager.getAudioDeviceSetup();

    if (native.bufferSize == bufferSize
        && getCurrentBufferSize() == bufferSize)
        return true;

    native.bufferSize = bufferSize;

    if (! applyNativeSetup (native, errorMessage))
        return false;

    const int actual = getCurrentBufferSize();

    if (actual != bufferSize)
    {
        if (errorMessage != nullptr)
            *errorMessage = "device selected " + juce::String (actual)
                            + " samples instead of " + juce::String (bufferSize);

        return false;
    }

    return true;
}

bool DeviceManager::setSampleRate (double sampleRate, juce::String* errorMessage)
{
    if (! isOpen())
    {
        if (errorMessage != nullptr)
            *errorMessage = "no audio device is open";

        return false;
    }

    auto native = deviceManager.getAudioDeviceSetup();

    if (sameRate (native.sampleRate, sampleRate)
        && sameRate (getCurrentSampleRate(), sampleRate))
        return true;

    native.sampleRate = sampleRate;

    if (! applyNativeSetup (native, errorMessage))
        return false;

    if (! sameRate (getCurrentSampleRate(), sampleRate))
    {
        if (errorMessage != nullptr)
            *errorMessage = "device selected " + juce::String (getCurrentSampleRate())
                            + " Hz instead of " + juce::String (sampleRate) + " Hz";

        return false;
    }

    return true;
}

}
