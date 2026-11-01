#include "audio/DeviceManager.h"

namespace AudioEngine
{

DeviceSetup DeviceManager::getCurrentSetup() const
{
    DeviceSetup setup;

    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        const auto native = deviceManager.getAudioDeviceSetup();
        setup.inputDeviceName = native.inputDeviceName;
        setup.outputDeviceName = native.outputDeviceName;
        setup.sampleRate = device->getCurrentSampleRate();
        setup.bufferSize = device->getCurrentBufferSizeSamples();
        setup.inputChannels = device->getActiveInputChannels();
        setup.outputChannels = device->getActiveOutputChannels();
        setup.numInputChannels = setup.inputChannels.countNumberOfSetBits();
        setup.numOutputChannels = setup.outputChannels.countNumberOfSetBits();
    }

    setup.deviceTypeName = getCurrentDeviceTypeName();
    return setup;
}

juce::String DeviceManager::getCurrentDeviceTypeName() const
{
    return deviceManager.getCurrentAudioDeviceType();
}

juce::String DeviceManager::getCurrentDeviceName() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getName();

    return {};
}

double DeviceManager::getCurrentSampleRate() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getCurrentSampleRate();

    return 0.0;
}

int DeviceManager::getCurrentBufferSize() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getCurrentBufferSizeSamples();

    return 0;
}

int DeviceManager::getCurrentBitDepth() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getCurrentBitDepth();

    return 0;
}

}
