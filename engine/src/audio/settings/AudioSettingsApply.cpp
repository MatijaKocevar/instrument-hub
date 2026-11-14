#include "audio/AudioSettings.h"

#include "audio/device/DeviceErrors.h"
#include "audio/settings/AudioProfiles.h"
#include "audio/settings/AudioSettingsChannels.h"
#include "audio/settings/AudioSettingsPersistence.h"

#include <algorithm>
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

        Api::AudioSettingsPatch request = patch;
        auto& manager = liveManager ();

        if (devices.empty ())
            refreshDevices ();

        if (devices.empty ())
        {
            error = "no audio device types are available";
            return false;
        }

        const bool wasOpen = manager.isOpen ();
        const auto currentType = wasOpen ? manager.getCurrentDeviceTypeName () : pendingType;
        const auto currentName = wasOpen ? manager.getCurrentDeviceName () : pendingName;

        auto targetType =
            request.deviceType.has_value () ? juce::String (*request.deviceType) : currentType;

        if (targetType.isEmpty ())
            targetType = firstDeviceType ();

        auto targetName =
            request.deviceName.has_value () ? juce::String (*request.deviceName) : currentName;

        if ((!request.deviceName.has_value () && targetType != currentType) ||
            targetName.isEmpty ())
        {
            if (findDevice (targetType, targetName) == nullptr)
                targetName = firstOutputDeviceOfType (targetType);
        }

        const CachedDevice* info = findDevice (targetType, targetName);
        CachedDevice rawDevice;

        if (info == nullptr && request.deviceName.has_value ())
        {
            for (const auto& raw : manager.getRawPcmDevices ())
            {
                if (raw.typeName != targetType || raw.name != targetName)
                    continue;

                rawDevice.type = raw.typeName;
                rawDevice.name = raw.name;
                rawDevice.available = true;
                rawDevice.hasOutput = true;
                info = &rawDevice;
                break;
            }
        }

        if (info != nullptr && !info->available)
        {
            refreshDevices ();
            info = findDevice (targetType, targetName);
        }

        if (info == nullptr || !info->hasOutput)
        {
            error = "the device is not connected";
            return false;
        }

        if (!info->available)
        {
            error = detail::describeDeviceError (info->unavailableReason).toStdString ();

            if (error.empty ())
                error = "the device is not connected";

            return false;
        }

        const bool selectingDevice =
            request.deviceType.has_value () || request.deviceName.has_value ();
        const bool explicitFormat =
            request.sampleRate.has_value () || request.bufferSize.has_value () ||
            request.inputChannels.has_value () || request.outputChannels.has_value ();

        if (selectingDevice && !explicitFormat && settingsStore != nullptr)
        {
            const auto& profiles = settingsStore->settings ().audio.profiles;

            if (const auto* profile = detail::findAudioProfile (profiles, targetType, targetName);
                profile != nullptr)
            {
                if (profile->sampleRate > 0)
                    request.sampleRate = static_cast<double> (profile->sampleRate);

                if (profile->bufferSize > 0)
                    request.bufferSize = profile->bufferSize;

                const auto channelsFit = [] (const std::vector<int>& channels, int available)
                {
                    if (channels.empty () || available <= 0)
                        return false;

                    return std::all_of (channels.begin (),
                                        channels.end (),
                                        [available] (int channel)
                                        { return channel >= 0 && channel < available; });
                };

                if (channelsFit (profile->inputChannels, info->inputNames.size ()))
                    request.inputChannels = profile->inputChannels;

                if (channelsFit (profile->outputChannels, info->outputNames.size ()))
                    request.outputChannels = profile->outputChannels;
            }
        }

        const bool deviceChanged =
            !wasOpen || targetType != currentType || targetName != currentName;
        const bool channelChange =
            request.inputChannels.has_value () || request.outputChannels.has_value ();

        if (channelChange)
        {
            if (request.outputChannels.has_value () &&
                !detail::validateChannels (
                    *request.outputChannels, info->outputNames.size (), "outputChannels", error))
            {
                return false;
            }

            if (request.inputChannels.has_value () && !request.inputChannels->empty () &&
                !detail::validateChannels (
                    *request.inputChannels, info->inputNames.size (), "inputChannels", error))
            {
                return false;
            }
        }

        DeviceSetup setup = wasOpen ? manager.getCurrentSetup () : DeviceSetup{};
        setup.deviceTypeName = targetType;
        setup.outputDeviceName = targetName;
        setup.inputDeviceName = info->hasInput ? targetName : juce::String ();
        setup.sampleRate =
            request.sampleRate.value_or (wasOpen ? manager.getCurrentSampleRate () : 48000.0);
        setup.bufferSize =
            request.bufferSize.value_or (wasOpen ? manager.getCurrentBufferSize () : 128);

        const int availableOutputs = info->outputNames.size ();
        const int availableInputs = info->hasInput ? info->inputNames.size () : 0;
        const bool channelsNeededProbe =
            info->outputNames.isEmpty () || (info->hasInput && info->inputNames.isEmpty ());

        const auto outputMap = detail::resolveChannelMap (
            request.outputChannels, setup.outputChannels, availableOutputs, 2, deviceChanged);

        setup.outputChannels = outputMap.mask;
        setup.numOutputChannels = outputMap.count;

        const auto inputMap = detail::resolveChannelMap (request.inputChannels,
                                                         setup.inputChannels,
                                                         availableInputs,
                                                         availableInputs,
                                                         deviceChanged);

        setup.inputChannels = inputMap.mask;
        setup.numInputChannels = inputMap.count;

        if (request.outputChannels.has_value () && setup.numOutputChannels <= 0)
        {
            error = "outputChannels must select at least one channel";
            return false;
        }

        if (!wasOpen || deviceChanged || channelChange)
        {
            juce::String openError;

            if (!manager.open (setup, &openError))
            {
                error = openError.isNotEmpty () ? openError.toStdString ()
                                                : "could not open the audio device";
                return false;
            }

            if (deviceChanged && channelsNeededProbe)
                refreshDevices ();
        }
        else
        {
            juce::String changeError;

            if (request.sampleRate.has_value () &&
                !manager.setSampleRate (*request.sampleRate, &changeError))
            {
                error = changeError.toStdString ();
                return false;
            }

            if (request.bufferSize.has_value () &&
                !manager.setBufferSize (*request.bufferSize, &changeError))
            {
                error = changeError.toStdString ();
                return false;
            }
        }

        if (request.sampleRate.has_value () &&
            std::abs (manager.getCurrentSampleRate () - *request.sampleRate) >= kLatencyEpsilon)
        {
            error = "the device did not accept the requested sample rate";
            return false;
        }

        if (request.bufferSize.has_value () &&
            manager.getCurrentBufferSize () != *request.bufferSize)
        {
            error = "the device did not accept the requested buffer size";
            return false;
        }

        pendingType = targetType;
        pendingName = targetName;
        pendingSampleRate = manager.getCurrentSampleRate ();
        pendingBufferSize = manager.getCurrentBufferSize ();

        restoreState = Api::AudioRestoreState{};

        if (settingsStore != nullptr)
        {
            const bool saved = persistAudioSettings (*settingsStore, manager);
            restoreState.usedStoredDevice = saved;

            if (!saved)
                restoreState.reason = "the audio device was applied but could not be saved";
        }

        return true;
    }

}
