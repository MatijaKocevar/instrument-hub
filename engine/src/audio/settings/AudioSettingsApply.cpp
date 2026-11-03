#include "audio/AudioSettings.h"

#include "audio/device/DeviceErrors.h"
#include "audio/settings/AudioSettingsChannels.h"
#include "audio/settings/AudioSettingsPersistence.h"

#include <cmath>

namespace AudioEngine
{

namespace
{
constexpr double kLatencyEpsilon = 0.5;
}

bool AudioSettingsBridge::apply (const Api::AudioSettingsPatch& patch, std::string& error)
{
    const std::lock_guard<std::mutex> lock (mutex);

    auto& manager = liveManager();

    if (devices.empty())
        refreshDevices();

    if (devices.empty())
    {
        error = "no audio device types are available";
        return false;
    }

    const bool wasOpen = manager.isOpen();
    const auto currentType = wasOpen ? manager.getCurrentDeviceTypeName() : pendingType;
    const auto currentName = wasOpen ? manager.getCurrentDeviceName() : pendingName;

    auto targetType = patch.deviceType.has_value()
                          ? juce::String (*patch.deviceType)
                          : currentType;

    if (targetType.isEmpty())
        targetType = firstDeviceType();

    auto targetName = patch.deviceName.has_value()
                          ? juce::String (*patch.deviceName)
                          : currentName;

    if ((! patch.deviceName.has_value() && targetType != currentType)
        || targetName.isEmpty())
    {
        if (findDevice (targetType, targetName) == nullptr)
            targetName = firstOutputDeviceOfType (targetType);
    }

    const CachedDevice* info = findDevice (targetType, targetName);

    if (info != nullptr && ! info->available)
    {
        refreshDevices();
        info = findDevice (targetType, targetName);
    }

    if (info == nullptr || ! info->hasOutput)
    {
        error = "the device is not connected";
        return false;
    }

    if (! info->available)
    {
        error = detail::describeDeviceError (info->unavailableReason).toStdString();

        if (error.empty())
            error = "the device is not connected";

        return false;
    }

    const bool deviceChanged = ! wasOpen
                               || targetType != currentType
                               || targetName != currentName;
    const bool channelChange = patch.inputChannels.has_value()
                               || patch.outputChannels.has_value();

    if (channelChange)
    {
        if (patch.outputChannels.has_value()
            && ! detail::validateChannels (*patch.outputChannels,
                                           info->outputNames.size(),
                                           "outputChannels", error))
        {
            return false;
        }

        if (patch.inputChannels.has_value()
            && ! patch.inputChannels->empty()
            && ! detail::validateChannels (*patch.inputChannels,
                                           info->inputNames.size(),
                                           "inputChannels", error))
        {
            return false;
        }
    }

    DeviceSetup setup = wasOpen ? manager.getCurrentSetup() : DeviceSetup {};
    setup.deviceTypeName = targetType;
    setup.outputDeviceName = targetName;
    setup.inputDeviceName = info->hasInput ? targetName : juce::String();
    setup.sampleRate = patch.sampleRate.value_or (wasOpen ? manager.getCurrentSampleRate()
                                                          : 48000.0);
    setup.bufferSize = patch.bufferSize.value_or (wasOpen ? manager.getCurrentBufferSize()
                                                          : 128);

    const int availableOutputs = info->outputNames.size();
    const int availableInputs = info->hasInput ? info->inputNames.size() : 0;
    const bool channelsNeededProbe = info->outputNames.isEmpty()
                                     || (info->hasInput && info->inputNames.isEmpty());

    const auto outputMap = detail::resolveChannelMap (
        patch.outputChannels,
        setup.outputChannels,
        availableOutputs,
        2,
        deviceChanged);

    setup.outputChannels = outputMap.mask;
    setup.numOutputChannels = outputMap.count;

    const auto inputMap = detail::resolveChannelMap (
        patch.inputChannels,
        setup.inputChannels,
        availableInputs,
        availableInputs,
        deviceChanged);

    setup.inputChannels = inputMap.mask;
    setup.numInputChannels = inputMap.count;

    if (patch.outputChannels.has_value() && setup.numOutputChannels <= 0)
    {
        error = "outputChannels must select at least one channel";
        return false;
    }

    if (! wasOpen || deviceChanged || channelChange)
    {
        juce::String openError;

        if (! manager.open (setup, &openError))
        {
            error = openError.isNotEmpty() ? openError.toStdString()
                                           : "could not open the audio device";
            return false;
        }

        if (deviceChanged && channelsNeededProbe)
            refreshDevices();
    }
    else
    {
        juce::String changeError;

        if (patch.sampleRate.has_value()
            && ! manager.setSampleRate (*patch.sampleRate, &changeError))
        {
            error = changeError.toStdString();
            return false;
        }

        if (patch.bufferSize.has_value()
            && ! manager.setBufferSize (*patch.bufferSize, &changeError))
        {
            error = changeError.toStdString();
            return false;
        }
    }

    if (patch.sampleRate.has_value()
        && std::abs (manager.getCurrentSampleRate() - *patch.sampleRate)
               >= kLatencyEpsilon)
    {
        error = "the device did not accept the requested sample rate";
        return false;
    }

    if (patch.bufferSize.has_value()
        && manager.getCurrentBufferSize() != *patch.bufferSize)
    {
        error = "the device did not accept the requested buffer size";
        return false;
    }

    pendingType = targetType;
    pendingName = targetName;
    pendingSampleRate = manager.getCurrentSampleRate();
    pendingBufferSize = manager.getCurrentBufferSize();

    restoreState = Api::AudioRestoreState {};

    if (settingsStore != nullptr)
    {
        const bool saved = persistAudioSettings (*settingsStore, manager);
        restoreState.usedStoredDevice = saved;

        if (! saved)
            restoreState.reason = "the audio device was applied but could not be saved";
    }

    return true;
}

}
