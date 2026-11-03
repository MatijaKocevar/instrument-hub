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

std::unique_ptr<juce::AudioIODevice> DeviceManager::createRawDevice (
    const juce::String& typeName,
    const juce::String& outputName,
    const juce::String& inputName)
{
    auto* type = findDeviceType (typeName);

    if (type == nullptr)
        return {};

    const auto inputNames = type->getDeviceNames (true);
    const auto outputNames = type->getDeviceNames (false);

    const bool hasInput = inputName.isNotEmpty() && inputNames.contains (inputName);
    const bool hasOutput = outputName.isNotEmpty() && outputNames.contains (outputName);

    if (! hasInput && ! hasOutput)
        return {};

    return std::unique_ptr<juce::AudioIODevice> (
        type->createDevice (hasOutput ? outputName : juce::String(),
                            hasInput ? inputName : juce::String()));
}

juce::String DeviceManager::resolveListedName (const juce::String& typeName,
                                               const juce::String& deviceName)
{
    if (deviceName.isEmpty())
        return {};

    for (const auto& info : getDevices())
    {
        if (info.typeName != typeName)
            continue;

        if (info.name == deviceName || info.juceName == deviceName)
            return info.juceName.isNotEmpty() ? info.juceName : info.name;
    }

    return deviceName;
}

} // namespace AudioEngine
