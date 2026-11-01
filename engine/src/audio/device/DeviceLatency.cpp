#include "audio/DeviceManager.h"

namespace AudioEngine
{

int DeviceManager::getInputLatencyInSamples() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getInputLatencyInSamples();

    return 0;
}

int DeviceManager::getOutputLatencyInSamples() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getOutputLatencyInSamples();

    return 0;
}

int DeviceManager::getXRunCount() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getXRunCount();

    return 0;
}

}
