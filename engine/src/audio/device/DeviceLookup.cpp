#include "audio/DeviceManager.h"

namespace AudioEngine
{

//==============================================================================
// Device type and device lookup shared by enumeration and opening.

juce::AudioIODeviceType* DeviceManager::findDeviceType (const juce::String& typeName)
{
    ensureInitialised();

    for (auto* type : deviceManager.getAvailableDeviceTypes())
    {
        if (type->getTypeName() == typeName)
        {
            type->scanForDevices();
            return type;
        }
    }

    return nullptr;
}

std::unique_ptr<juce::AudioIODevice> DeviceManager::createDevice (const juce::String& typeName,
                                                                  const juce::String& deviceName)
{
    auto* type = findDeviceType (typeName);

    if (type == nullptr || deviceName.isEmpty())
        return {};

    const auto inputNames = type->getDeviceNames (true);
    const auto outputNames = type->getDeviceNames (false);

    const bool hasInput = inputNames.contains (deviceName);
    const bool hasOutput = outputNames.contains (deviceName);

    if (! hasInput && ! hasOutput)
        return {};

    // ALSA keeps separate input and output name lists; passing the matching
    // name for both sides lets JUCE report the rates for the combined device.
    return std::unique_ptr<juce::AudioIODevice> (
        type->createDevice (hasOutput ? deviceName : juce::String(),
                            hasInput ? deviceName : juce::String()));
}

} // namespace AudioEngine
