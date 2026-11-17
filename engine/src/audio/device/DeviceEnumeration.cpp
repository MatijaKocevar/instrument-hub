#include "audio/DeviceManager.h"

namespace AudioEngine {

juce::StringArray DeviceManager::getDeviceTypeNames() {
    ensureInitialised();

    juce::StringArray names;

    for (auto* type : deviceManager.getAvailableDeviceTypes()) {
        type->scanForDevices();
        names.add(type->getTypeName());
    }

    return names;
}

std::vector<DeviceTypeInfo> DeviceManager::getDeviceTypes() {
    ensureInitialised();

    std::vector<DeviceTypeInfo> result;

    for (auto* type : deviceManager.getAvailableDeviceTypes()) {
        type->scanForDevices();

        DeviceTypeInfo info;
        info.name = type->getTypeName();
        info.inputDeviceNames = type->getDeviceNames(true);
        info.outputDeviceNames = type->getDeviceNames(false);
        result.push_back(std::move(info));
    }

    return result;
}

std::vector<DeviceInfo> DeviceManager::getDevices() {
    ensureInitialised();

    if (listOptions.raw) return getRawDevices();

    if (!cardCacheValid) {
        cardCache = getCardDevices();

        auto system = getSystemDevice();

        for (auto& device : system) cardCache.push_back(std::move(device));

        auto extraTypes = getTypeDevices();

        for (auto& device : extraTypes) cardCache.push_back(std::move(device));

        cardCacheValid = true;
    }

    return cardCache;
}

std::vector<DeviceInfo> DeviceManager::getRawDevices() {
    std::vector<DeviceInfo> result;

    for (auto* type : deviceManager.getAvailableDeviceTypes()) {
        type->scanForDevices();

        const auto inputNames = type->getDeviceNames(true);
        const auto outputNames = type->getDeviceNames(false);
        const int defaultInput = type->getDefaultDeviceIndex(true);
        const int defaultOutput = type->getDefaultDeviceIndex(false);

        juce::StringArray allNames(inputNames);

        for (const auto& name : outputNames)
            if (!allNames.contains(name)) allNames.add(name);

        for (const auto& name : allNames) {
            auto device = createRawDevice(type->getTypeName(), name, name);

            if (device == nullptr) continue;

            DeviceInfo info;
            info.typeName = type->getTypeName();
            info.name = name;
            info.juceName = name;
            info.hasInput = inputNames.contains(name);
            info.hasOutput = outputNames.contains(name);
            info.isDefaultInput =
                info.hasInput && defaultInput >= 0 && inputNames[defaultInput] == name;
            info.isDefaultOutput =
                info.hasOutput && defaultOutput >= 0 && outputNames[defaultOutput] == name;

            info.inputChannelNames = device->getInputChannelNames();
            info.outputChannelNames = device->getOutputChannelNames();
            info.inputChannelMask.setRange(0, info.inputChannelNames.size(), true);
            info.outputChannelMask.setRange(0, info.outputChannelNames.size(), true);

            info.sampleRates = device->getAvailableSampleRates();
            info.bufferSizes = device->getAvailableBufferSizes();

            result.push_back(std::move(info));
        }
    }

    return result;
}

juce::Array<double> DeviceManager::getAvailableSampleRates(const juce::String& typeName,
                                                           const juce::String& deviceName) {
    const auto name = resolveListedName(typeName, deviceName);

    if (auto device = createRawDevice(typeName, name, name))
        return device->getAvailableSampleRates();

    return {};
}

juce::Array<int> DeviceManager::getAvailableBufferSizes(const juce::String& typeName,
                                                        const juce::String& deviceName) {
    const auto name = resolveListedName(typeName, deviceName);

    if (auto device = createRawDevice(typeName, name, name))
        return device->getAvailableBufferSizes();

    return {};
}

}  // namespace AudioEngine
