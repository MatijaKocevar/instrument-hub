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

    int activeOutputs = devices.getActiveOutputChannels();

    for (int attempt = 0;
         setup.deviceTypeName == "JACK"
         && activeOutputs < juce::jmax (1, config.device.numOutputChannels)
         && attempt < 3;
         ++attempt)
    {
        closeDevice();
        juce::Thread::sleep (100);

        if (! devices.resolve (config.device, setup, error))
            return false;

        if (! devices.open (setup, error))
            return false;

        activeOutputs = devices.getActiveOutputChannels();
    }

    const double actualRate = devices.getCurrentSampleRate();
    const int actualBlock = devices.getCurrentBufferSize();

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

bool Session::openDevice (const SessionDeviceConfig& device,
                          juce::String* errorMessage)
{
    config.device = device;
    return openDevice (errorMessage);
}

void Session::closeDevice()
{
    devices.close();
}

void Session::beginShutdown()
{
    shuttingDown.store (true, std::memory_order_release);
    cancelPendingLoads();
}

void Session::cancelPendingLoads()
{
    const auto token = loadAbort;

    if (token != nullptr)
        token->store (true, std::memory_order_release);
}

}
