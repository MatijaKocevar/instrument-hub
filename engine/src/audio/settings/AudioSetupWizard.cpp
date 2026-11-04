#include "audio/AudioSettings.h"

#include "audio/settings/AudioProfiles.h"
#include "audio/settings/AudioSetupRecommendation.h"
#include "audio/settings/LatencyBudget.h"

#include <algorithm>
#include <vector>

namespace AudioEngine
{

namespace
{
constexpr int probeBuffers[] = { 64, 96, 128 };
constexpr int probeSettleMs = 200;

std::vector<double> probeSampleRates (const juce::Array<double>& available,
                                      double current)
{
    std::vector<double> rates;

    const auto add = [&rates] (double rate)
    {
        if (rate <= 0.0)
            return;

        if (std::find (rates.begin(), rates.end(), rate) == rates.end())
            rates.push_back (rate);
    };

    add (current);

    for (const auto rate : available)
        add (rate);

    if (rates.empty())
        add (48000.0);

    if (rates.size() > 1)
        std::sort (rates.begin() + 1, rates.end());

    if (rates.size() > 4)
        rates.resize (4);

    return rates;
}

void fillSetupChannels (DeviceSetup& setup,
                        int outputNames,
                        bool hasInput,
                        int inputNames,
                        bool sameDevice,
                        const DeviceSetup& saved)
{
    const int outputs = outputNames <= 0 ? 2 : juce::jmin (2, outputNames);
    const int inputs = hasInput ? (inputNames <= 0 ? 2 : juce::jmin (2, inputNames))
                                : 0;

    setup.numOutputChannels = outputs;
    setup.numInputChannels = inputs;
    setup.outputChannels = DeviceSetup::channelMask (outputs);
    setup.inputChannels = DeviceSetup::channelMask (inputs);

    if (! sameDevice)
        return;

    if (! saved.outputChannels.isZero())
    {
        setup.outputChannels = saved.outputChannels;
        setup.numOutputChannels = saved.outputChannels.countNumberOfSetBits();
    }

    if (! saved.inputChannels.isZero())
    {
        setup.inputChannels = saved.inputChannels;
        setup.numInputChannels = saved.inputChannels.countNumberOfSetBits();
    }
}

Api::AudioProbeState probeCandidate (DeviceManager& manager,
                                     const DeviceSetup& setup,
                                     int chainLatencySamples,
                                     const Api::AudioMeasureHandler& measureDevice)
{
    Api::AudioProbeState probe;
    probe.sampleRate = setup.sampleRate;
    probe.bufferSize = setup.bufferSize;

    juce::String openError;

    if (! manager.open (setup, &openError))
    {
        probe.error = openError.toStdString();
        return probe;
    }

    probe.opened = true;
    juce::Thread::sleep (probeSettleMs);

    const auto actualRate = manager.getCurrentSampleRate();
    const auto actualBuffer = manager.getCurrentBufferSize();

    if (actualRate > 0.0)
        probe.sampleRate = actualRate;

    if (actualBuffer > 0)
        probe.bufferSize = actualBuffer;

    if (measureDevice != nullptr
        && ! manager.getActiveInputChannels().isZero())
    {
        const auto measurement = measureDevice();

        if (measurement.valid)
        {
            probe.measuredMs = measurement.roundTripMs;
            probe.expectedMs = measurement.expectedMs;
        }
        else if (! measurement.error.empty())
        {
            probe.error = measurement.error;
        }
    }

    probe.xruns = static_cast<std::uint64_t> (juce::jmax (0, manager.getXRunCount()));
    probe.stable = probe.xruns == 0;

    const auto deviceLatency = manager.getInputLatencyInSamples()
                             + manager.getOutputLatencyInSamples();
    probe.latencyMs = detail::computeLatencyBudget (probe.bufferSize,
                                                     probe.sampleRate,
                                                     deviceLatency,
                                                     chainLatencySamples)
                          .totalMs;

    if (probe.measuredMs <= 0.0)
        probe.expectedMs = static_cast<double> (probe.bufferSize) * 2000.0
                           / probe.sampleRate;

    return probe;
}
}

Api::AudioWizardState AudioSettingsBridge::recommendSetup()
{
    const std::lock_guard<std::mutex> lock (mutex);

    auto& manager = liveManager();

    if (devices.empty())
        refreshDevices();

    Api::AudioWizardState result;

    if (devices.empty())
    {
        result.error = "no audio device types are available";
        return result;
    }

    const bool wasOpen = manager.isOpen();
    const auto saved = wasOpen ? manager.getCurrentSetup() : DeviceSetup {};
    auto targetType = wasOpen ? manager.getCurrentDeviceTypeName() : pendingType;
    auto targetName = wasOpen ? manager.getCurrentDeviceName() : pendingName;

    if (targetType.isEmpty())
        targetType = firstDeviceType();

    if (targetName.isEmpty())
        targetName = firstOutputDeviceOfType (targetType);

    const auto* info = findDevice (targetType, targetName);

    if (info == nullptr || ! info->hasOutput)
    {
        result.error = "the selected audio device is not available for testing";
        return result;
    }

    result.deviceType = targetType.toStdString();
    result.deviceName = targetName.toStdString();
    result.soundcard = detail::soundcardKeyFor (targetName).toStdString();
    result.profile = detail::audioProfileLabel (targetType, targetName).toStdString();

    const bool sameDevice = wasOpen
                            && targetType == manager.getCurrentDeviceTypeName()
                            && targetName == manager.getCurrentDeviceName();

    DeviceSetup base = sameDevice ? saved : DeviceSetup {};
    base.deviceTypeName = targetType;
    base.outputDeviceName = targetName;
    base.inputDeviceName = info->hasInput ? targetName : juce::String();
    fillSetupChannels (base,
                       info->outputNames.size(),
                       info->hasInput,
                       info->inputNames.size(),
                       sameDevice,
                       saved);

    result.inputChannels = detail::activeChannelList (base.inputChannels);
    result.outputChannels = detail::activeChannelList (base.outputChannels);

    auto availableRates = info->sampleRates;

    if (availableRates.isEmpty())
        availableRates = manager.getAvailableSampleRates (targetType, targetName);

    const auto rates = probeSampleRates (availableRates,
                                         wasOpen ? manager.getCurrentSampleRate()
                                                 : 48000.0);
    const int chainLatency = chainLatencySamples.load (std::memory_order_relaxed);

    for (const auto rate : rates)
    {
        for (const auto buffer : probeBuffers)
        {
            auto setup = base;
            setup.sampleRate = rate;
            setup.bufferSize = buffer;

            result.candidates.push_back (probeCandidate (manager,
                                                         setup,
                                                         chainLatency,
                                                         deviceMeasure));
        }
    }

    juce::String restoreError;

    if (wasOpen)
    {
        if (! manager.open (saved, &restoreError))
            result.error = "could not restore the original audio device: "
                           + restoreError.toStdString();
    }
    else
    {
        manager.close();
    }

    const auto restoreWarning = result.error;

    detail::finalizeAudioWizard (result, wasOpen ? saved.sampleRate : 48000.0);

    if (! restoreWarning.empty())
    {
        if (result.valid)
            result.tradeoff += " Warning: " + restoreWarning;
        else
            result.error += "; " + restoreWarning;
    }

    lastRecommendation = result;
    return result;
}

}
