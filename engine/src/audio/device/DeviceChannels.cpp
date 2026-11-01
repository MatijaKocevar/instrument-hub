#include "audio/DeviceManager.h"

namespace AudioEngine
{

//==============================================================================
// Channel maps: the setup mask builder and the masks of the open device.

juce::BigInteger DeviceSetup::channelMask (int numChannels)
{
    juce::BigInteger mask;
    mask.setRange (0, juce::jmax (0, numChannels), true);
    return mask;
}

juce::BigInteger DeviceManager::getActiveInputChannels() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getActiveInputChannels();

    return {};
}

juce::BigInteger DeviceManager::getActiveOutputChannels() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getActiveOutputChannels();

    return {};
}

juce::BigInteger DeviceManager::getAvailableInputChannelMask() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        juce::BigInteger mask;
        mask.setRange (0, device->getInputChannelNames().size(), true);
        return mask;
    }

    return {};
}

juce::BigInteger DeviceManager::getAvailableOutputChannelMask() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        juce::BigInteger mask;
        mask.setRange (0, device->getOutputChannelNames().size(), true);
        return mask;
    }

    return {};
}

} // namespace AudioEngine
