#include "Session.h"

#include <algorithm>

namespace ihub
{

bool Session::openDevice (juce::String* errorMessage)
{
    juce::String localError;
    auto& error = errorMessage != nullptr ? *errorMessage : localError;
    error.clear();

    closeDevice();

    if (engine == nullptr || ! prepared)
    {
        error = "the session graph has not been built";
        return false;
    }

    AudioEngine::DeviceSetup setup;

    if (! devices.resolve (config.device, setup, error))
        return false;

    if (! devices.open (setup, error))
        return false;

    const double actualRate = devices.getCurrentSampleRate();
    const int actualBlock = devices.getCurrentBufferSize();
    const int activeOutputs = devices.getActiveOutputChannels();

    numChannels = activeOutputs > 0
                      ? juce::jmin (config.device.numOutputChannels, activeOutputs)
                      : config.device.numOutputChannels;

    prepare (actualRate > 0.0 ? actualRate : config.device.sampleRate,
             actualBlock > 0 ? juce::jmax (actualBlock, config.device.bufferSize)
                             : config.device.bufferSize,
             numChannels);

    devices.setCallback (this);
    return true;
}

void Session::closeDevice()
{
    devices.close();
}

} // namespace ihub
