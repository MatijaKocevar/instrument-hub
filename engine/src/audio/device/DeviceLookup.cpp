#include "audio/DeviceManager.h"

namespace AudioEngine
{

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

    return std::unique_ptr<juce::AudioIODevice> (
        type->createDevice (hasOutput ? deviceName : juce::String(),
                            hasInput ? deviceName : juce::String()));
}

}
