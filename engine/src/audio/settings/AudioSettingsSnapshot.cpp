#include "audio/AudioSettings.h"

#include "audio/settings/LatencyBudget.h"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace AudioEngine
{

namespace
{
std::string toString (const juce::String& text)
{
    return text.toStdString();
}
}

Api::AudioStateSnapshot AudioSettingsBridge::snapshot()
{
    const std::lock_guard<std::mutex> lock (mutex);

    auto& manager = liveManager();

    if (devices.empty())
        refreshDevices();

    Api::AudioStateSnapshot result;
    result.open = manager.isOpen();

    for (const auto& device : devices)
    {
        Api::AudioDeviceState entry;
        entry.type = toString (device.type);
        entry.name = toString (device.name);
        result.devices.push_back (std::move (entry));

        const auto type = toString (device.type);

        if (std::find (result.deviceTypes.begin(), result.deviceTypes.end(), type)
            == result.deviceTypes.end())
        {
            result.deviceTypes.push_back (type);
        }
    }

    const auto currentType = result.open ? manager.getCurrentDeviceTypeName()
                                         : pendingType;
    const auto currentName = result.open ? manager.getCurrentDeviceName()
                                         : pendingName;

    result.deviceType = toString (currentType);
    result.deviceName = toString (currentName);

    double sampleRate = result.open ? manager.getCurrentSampleRate()
                                    : pendingSampleRate;
    int bufferSize = result.open ? manager.getCurrentBufferSize()
                                 : pendingBufferSize;

    if (sampleRate <= 0.0)
        sampleRate = 48000.0;

    if (bufferSize <= 0)
        bufferSize = 128;

    result.sampleRate = sampleRate;
    result.bufferSize = bufferSize;

    const auto* info = findDevice (currentType, currentName);

    if (info != nullptr)
    {
        for (const auto rate : info->sampleRates)
            result.sampleRates.push_back (rate);

        for (const auto size : info->bufferSizes)
            result.bufferSizes.push_back (size);
    }

    const auto inputMask = result.open ? manager.getActiveInputChannels()
                                       : juce::BigInteger();
    const auto outputMask = result.open ? manager.getActiveOutputChannels()
                                        : juce::BigInteger();

    if (info != nullptr)
    {
        for (int index = 0; index < info->inputNames.size(); ++index)
            result.inputChannels.push_back ({ index,
                                              toString (info->inputNames[index]),
                                              inputMask[index] });

        for (int index = 0; index < info->outputNames.size(); ++index)
            result.outputChannels.push_back ({ index,
                                               toString (info->outputNames[index]),
                                               outputMask[index] });
    }

    const auto chainSamples = chainLatencySamples.load (std::memory_order_relaxed);
    const auto deviceLatencySamples = result.open
                                          ? manager.getInputLatencyInSamples()
                                                + manager.getOutputLatencyInSamples()
                                          : 0;

    result.latency = detail::computeLatencyBudget (bufferSize,
                                                   sampleRate,
                                                   deviceLatencySamples,
                                                   chainSamples);

    result.deviceXruns = result.open
                             ? static_cast<std::uint64_t> (
                                   juce::jmax (0, manager.getXRunCount()))
                             : 0;

    if (timingMonitor != nullptr)
    {
        result.callbackOverruns = timingMonitor->getOverrunCount();
        result.callbacks = timingMonitor->getCallbackCount();
        result.loadPercent = timingMonitor->getLastLoadPercent();
    }

    result.xruns = result.deviceXruns + result.callbackOverruns;
    return result;
}

}
