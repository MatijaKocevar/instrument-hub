#include "audio/settings/AudioDeviceSelection.h"

namespace AudioEngine::detail
{

namespace
{
bool nameMatches (const juce::String& candidate,
                  const juce::String& wanted,
                  bool exact)
{
    return exact ? candidate == wanted : candidate.containsIgnoreCase (wanted);
}

bool openWith (const AudioDevicePreference& preference,
               const AudioDeviceOpener& opener,
               juce::String& error)
{
    error.clear();
    return opener != nullptr && opener (preference, error);
}
}

const DeviceInfo* findOutputDevice (const std::vector<DeviceInfo>& devices,
                                    const juce::String& type,
                                    const juce::String& name,
                                    bool exactName)
{
    for (const auto& device : devices)
    {
        if (! device.hasOutput)
            continue;

        if (type.isNotEmpty() && device.typeName != type)
            continue;

        if (! nameMatches (device.name, name, exactName)
            && ! nameMatches (device.juceName, name, exactName)
            && ! nameMatches (device.pcmName, name, exactName))
            continue;

        return &device;
    }

    return nullptr;
}

const DeviceInfo* firstOutputDeviceOfType (const std::vector<DeviceInfo>& devices,
                                           const juce::String& type)
{
    for (const auto& device : devices)
        if (device.hasOutput && (type.isEmpty() || device.typeName == type))
            return &device;

    return nullptr;
}

const DeviceInfo* defaultOutputDevice (const std::vector<DeviceInfo>& devices)
{
    for (const auto& device : devices)
        if (device.hasOutput && device.isDefaultOutput)
            return &device;

    return nullptr;
}

AudioDevicePreference devicePreference (const DeviceInfo& device,
                                        double sampleRate,
                                        int bufferSize)
{
    AudioDevicePreference preference;
    preference.deviceType = device.typeName;
    preference.deviceName = device.name;
    preference.sampleRate = sampleRate > 0.0 ? sampleRate : defaultAudioSampleRate;
    preference.bufferSize = bufferSize > 0 ? bufferSize : defaultAudioBufferSize;
    return preference;
}

bool openFallbackDevice (const std::vector<DeviceInfo>& devices,
                         const DeviceInfo* skip,
                         double sampleRate,
                         int bufferSize,
                         const AudioDeviceOpener& opener,
                         AudioDevicePreference& preference,
                         juce::String& error)
{
    const DeviceInfo* candidates[] = {
        defaultOutputDevice (devices),
        firstOutputDeviceOfType (devices, "ALSA"),
        firstOutputDeviceOfType (devices, {})
    };

    for (const auto* candidate : candidates)
    {
        if (candidate == nullptr || candidate == skip)
            continue;

        const auto attempted = devicePreference (*candidate,
                                                 sampleRate,
                                                 bufferSize);

        if (openWith (attempted, opener, error))
        {
            preference = attempted;
            return true;
        }
    }

    return false;
}

}
