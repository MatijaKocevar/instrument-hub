#include "AudioSettings.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace AudioEngine
{

namespace
{
constexpr double kLatencyEpsilon = 0.5;

juce::BigInteger makeMask (const std::vector<int>& channels)
{
    juce::BigInteger mask;

    for (const auto channel : channels)
        if (channel >= 0)
            mask.setBit (channel, true);

    return mask;
}

bool validateChannels (const std::vector<int>& channels,
                       int available,
                       const char* name,
                       std::string& error)
{
    if (channels.empty())
    {
        error = std::string (name) + " must select at least one channel";
        return false;
    }

    for (const auto channel : channels)
    {
        if (channel < 0 || channel >= available)
        {
            error = std::string (name) + " contains channel "
                    + juce::String (channel).toStdString()
                    + " but the device only exposes "
                    + juce::String (available).toStdString();
            return false;
        }
    }

    return true;
}

std::string toString (const juce::String& text)
{
    return text.toStdString();
}
} // namespace

//==============================================================================
AudioSettingsBridge::AudioSettingsBridge()
{
    refreshDevices();

    if (devices.empty())
        return;

    pendingType = firstDeviceType();
    pendingName = firstOutputDeviceOfType (pendingType);
}

void AudioSettingsBridge::refreshDevices()
{
    devices.clear();

    for (const auto& info : manager.getDevices())
    {
        CachedDevice cached;
        cached.type = info.typeName;
        cached.name = info.name;
        cached.hasInput = info.hasInput;
        cached.hasOutput = info.hasOutput;
        cached.inputNames = info.inputChannelNames;
        cached.outputNames = info.outputChannelNames;
        cached.sampleRates = info.sampleRates;
        cached.bufferSizes = info.bufferSizes;
        devices.push_back (std::move (cached));
    }
}

const AudioSettingsBridge::CachedDevice* AudioSettingsBridge::findDevice (
    const juce::String& type,
    const juce::String& name) const
{
    for (const auto& device : devices)
        if (device.type == type && device.name == name)
            return &device;

    return nullptr;
}

juce::String AudioSettingsBridge::firstDeviceType() const
{
    for (const auto& device : devices)
        if (device.type == "ALSA")
            return device.type;

    return devices.empty() ? juce::String() : devices.front().type;
}

juce::String AudioSettingsBridge::firstOutputDeviceOfType (const juce::String& type) const
{
    for (const auto& device : devices)
        if (device.type == type && device.hasOutput)
            return device.name;

    for (const auto& device : devices)
        if (device.hasOutput)
            return device.name;

    return {};
}

bool AudioSettingsBridge::openLoopback (juce::String* errorMessage)
{
    const std::lock_guard<std::mutex> lock (mutex);

    if (devices.empty())
        refreshDevices();

    const CachedDevice* loopback = nullptr;

    for (const auto& device : devices)
        if (device.hasOutput && device.name.containsIgnoreCase ("Loopback"))
        {
            loopback = &device;
            break;
        }

    if (loopback == nullptr)
    {
        if (errorMessage != nullptr)
            *errorMessage = "no Loopback output device was enumerated";

        return false;
    }

    DeviceSetup setup;
    setup.deviceTypeName = loopback->type;
    setup.outputDeviceName = loopback->name;
    setup.inputDeviceName = loopback->hasInput ? loopback->name : juce::String();
    setup.sampleRate = 48000.0;
    setup.bufferSize = 128;
    setup.numOutputChannels = juce::jmin (2, loopback->outputNames.size());
    setup.numInputChannels = loopback->hasInput
                                 ? juce::jmin (2, loopback->inputNames.size())
                                 : 0;
    setup.outputChannels = DeviceSetup::channelMask (setup.numOutputChannels);
    setup.inputChannels = DeviceSetup::channelMask (setup.numInputChannels);

    if (! manager.open (setup, errorMessage))
        return false;

    pendingType = setup.deviceTypeName;
    pendingName = setup.outputDeviceName;
    pendingSampleRate = manager.getCurrentSampleRate();
    pendingBufferSize = manager.getCurrentBufferSize();
    return true;
}

bool AudioSettingsBridge::openFirstOutput (juce::String* errorMessage)
{
    const std::lock_guard<std::mutex> lock (mutex);

    if (devices.empty())
        refreshDevices();

    const CachedDevice* chosen = nullptr;

    for (const auto& device : devices)
        if (device.hasOutput)
        {
            chosen = &device;
            break;
        }

    if (chosen == nullptr)
    {
        if (errorMessage != nullptr)
            *errorMessage = "no output device was enumerated";

        return false;
    }

    DeviceSetup setup;
    setup.deviceTypeName = chosen->type;
    setup.outputDeviceName = chosen->name;
    setup.inputDeviceName = chosen->hasInput ? chosen->name : juce::String();
    setup.sampleRate = 48000.0;
    setup.bufferSize = 128;
    setup.numOutputChannels = juce::jmin (2, chosen->outputNames.size());
    setup.numInputChannels = chosen->hasInput
                                 ? juce::jmin (2, chosen->inputNames.size())
                                 : 0;
    setup.outputChannels = DeviceSetup::channelMask (setup.numOutputChannels);
    setup.inputChannels = DeviceSetup::channelMask (setup.numInputChannels);

    if (! manager.open (setup, errorMessage))
        return false;

    pendingType = setup.deviceTypeName;
    pendingName = setup.outputDeviceName;
    pendingSampleRate = manager.getCurrentSampleRate();
    pendingBufferSize = manager.getCurrentBufferSize();
    return true;
}

void AudioSettingsBridge::close()
{
    const std::lock_guard<std::mutex> lock (mutex);
    manager.close();
}

//==============================================================================
Api::AudioStateSnapshot AudioSettingsBridge::snapshot()
{
    const std::lock_guard<std::mutex> lock (mutex);

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

    result.latency.bufferMs = static_cast<double> (bufferSize) * 1000.0 / sampleRate;
    result.latency.deviceMs = result.open
                                  ? static_cast<double> (manager.getInputLatencyInSamples()
                                                         + manager.getOutputLatencyInSamples())
                                        * 1000.0 / sampleRate
                                  : 0.0;
    result.latency.chainMs = static_cast<double> (chainSamples) * 1000.0 / sampleRate;
    result.latency.totalMs = result.latency.bufferMs
                             + result.latency.deviceMs
                             + result.latency.chainMs;

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

//==============================================================================
bool AudioSettingsBridge::apply (const Api::AudioSettingsPatch& patch, std::string& error)
{
    const std::lock_guard<std::mutex> lock (mutex);

    if (devices.empty())
        refreshDevices();

    if (devices.empty())
    {
        error = "no audio device types are available";
        return false;
    }

    const bool wasOpen = manager.isOpen();
    const auto currentType = wasOpen ? manager.getCurrentDeviceTypeName() : pendingType;
    const auto currentName = wasOpen ? manager.getCurrentDeviceName() : pendingName;

    auto targetType = patch.deviceType.has_value()
                          ? juce::String (*patch.deviceType)
                          : currentType;

    if (targetType.isEmpty())
        targetType = firstDeviceType();

    auto targetName = patch.deviceName.has_value()
                          ? juce::String (*patch.deviceName)
                          : currentName;

    // Switching device type without naming a device picks that type's first
    // output, so the UI can change the type with one control.
    if ((! patch.deviceName.has_value() && targetType != currentType)
        || targetName.isEmpty())
    {
        if (findDevice (targetType, targetName) == nullptr)
            targetName = firstOutputDeviceOfType (targetType);
    }

    const auto* info = findDevice (targetType, targetName);

    if (info == nullptr || ! info->hasOutput)
    {
        error = "unknown audio output device: "
                + targetType.toStdString() + " / " + targetName.toStdString();
        return false;
    }

    const bool deviceChanged = ! wasOpen
                               || targetType != currentType
                               || targetName != currentName;
    const bool channelChange = patch.inputChannels.has_value()
                               || patch.outputChannels.has_value();

    if (channelChange)
    {
        if (patch.outputChannels.has_value()
            && ! validateChannels (*patch.outputChannels,
                                   info->outputNames.size(),
                                   "outputChannels", error))
        {
            return false;
        }

        if (patch.inputChannels.has_value()
            && ! patch.inputChannels->empty()
            && ! validateChannels (*patch.inputChannels,
                                   info->inputNames.size(),
                                   "inputChannels", error))
        {
            return false;
        }
    }

    DeviceSetup setup = wasOpen ? manager.getCurrentSetup() : DeviceSetup {};
    setup.deviceTypeName = targetType;
    setup.outputDeviceName = targetName;
    setup.inputDeviceName = info->hasInput ? targetName : juce::String();
    setup.sampleRate = patch.sampleRate.value_or (wasOpen ? manager.getCurrentSampleRate()
                                                          : 48000.0);
    setup.bufferSize = patch.bufferSize.value_or (wasOpen ? manager.getCurrentBufferSize()
                                                          : 128);

    if (patch.outputChannels.has_value())
    {
        setup.outputChannels = makeMask (*patch.outputChannels);
        setup.numOutputChannels = static_cast<int> (patch.outputChannels->size());
    }
    else if (deviceChanged || setup.outputChannels.isZero())
    {
        setup.numOutputChannels = juce::jmin (2, info->outputNames.size());
        setup.outputChannels = DeviceSetup::channelMask (setup.numOutputChannels);
    }
    else
    {
        setup.numOutputChannels = setup.outputChannels.countNumberOfSetBits();
    }

    if (patch.inputChannels.has_value())
    {
        setup.inputChannels = makeMask (*patch.inputChannels);
        setup.numInputChannels = static_cast<int> (patch.inputChannels->size());
    }
    else if (deviceChanged || setup.inputChannels.isZero())
    {
        setup.numInputChannels = info->hasInput
                                     ? juce::jmin (2, info->inputNames.size())
                                     : 0;
        setup.inputChannels = DeviceSetup::channelMask (setup.numInputChannels);
    }
    else
    {
        setup.numInputChannels = setup.inputChannels.countNumberOfSetBits();
    }

    if (setup.numOutputChannels <= 0)
    {
        error = "outputChannels must select at least one channel";
        return false;
    }

    if (! wasOpen || deviceChanged || channelChange)
    {
        juce::String openError;

        if (! manager.open (setup, &openError))
        {
            error = openError.isNotEmpty() ? openError.toStdString()
                                           : "could not open the audio device";
            return false;
        }
    }
    else
    {
        juce::String changeError;

        if (patch.sampleRate.has_value()
            && ! manager.setSampleRate (*patch.sampleRate, &changeError))
        {
            error = changeError.toStdString();
            return false;
        }

        if (patch.bufferSize.has_value()
            && ! manager.setBufferSize (*patch.bufferSize, &changeError))
        {
            error = changeError.toStdString();
            return false;
        }
    }

    if (patch.sampleRate.has_value()
        && std::abs (manager.getCurrentSampleRate() - *patch.sampleRate)
               >= kLatencyEpsilon)
    {
        error = "the device did not accept the requested sample rate";
        return false;
    }

    if (patch.bufferSize.has_value()
        && manager.getCurrentBufferSize() != *patch.bufferSize)
    {
        error = "the device did not accept the requested buffer size";
        return false;
    }

    pendingType = targetType;
    pendingName = targetName;
    pendingSampleRate = manager.getCurrentSampleRate();
    pendingBufferSize = manager.getCurrentBufferSize();
    return true;
}

//==============================================================================
Api::AudioMeasurementState AudioSettingsBridge::measure()
{
    const std::lock_guard<std::mutex> lock (mutex);

    Api::AudioMeasurementState result;

    LatencyProbeOptions options;
    options.sampleRate = manager.isOpen() ? manager.getCurrentSampleRate()
                                          : pendingSampleRate;
    options.bufferSize = manager.isOpen() ? manager.getCurrentBufferSize()
                                          : pendingBufferSize;

    if (! (options.sampleRate > 0.0))
        options.sampleRate = 48000.0;

    if (options.bufferSize <= 0)
        options.bufferSize = 128;

    LatencyProbe probe;
    LatencyResult measured;
    const bool wasOpen = manager.isOpen();

    // Subdevice 0 normally belongs to the running stream; 1..3 are free on the
    // ALSA Loopback card, so the probe can run without stopping audio.
    if (wasOpen)
    {
        for (int subdevice = 1; subdevice <= 3 && ! measured.valid; ++subdevice)
        {
            options.subdevice = subdevice;
            measured = probe.measure (options);
        }
    }

    if (! measured.valid)
    {
        const auto savedSetup = wasOpen ? manager.getCurrentSetup() : DeviceSetup {};

        if (wasOpen)
            manager.close();

        for (int subdevice = 0; subdevice < 4 && ! measured.valid; ++subdevice)
        {
            options.subdevice = subdevice;
            measured = probe.measure (options);
        }

        if (wasOpen)
        {
            juce::String reopenError;

            if (! manager.open (savedSetup, &reopenError) && measured.valid)
            {
                measured.error = measured.error.isNotEmpty()
                                     ? measured.error + "; could not reopen the device: "
                                           + reopenError
                                     : "could not reopen the device: " + reopenError;
            }
        }
    }

    result.valid = measured.valid;
    result.roundTripSamples = measured.roundTripSamples;
    result.roundTripMs = measured.roundTripMs;
    result.expectedMs = measured.expectedMs();
    result.confidence = measured.confidence;
    result.sampleRate = measured.sampleRate;
    result.bufferSize = measured.bufferSize;
    result.error = measured.error.toStdString();
    return result;
}

void AudioSettingsBridge::setChainLatencySamples (int samples) noexcept
{
    chainLatencySamples.store (juce::jmax (0, samples), std::memory_order_relaxed);
}

void AudioSettingsBridge::setTimingMonitor (TimingMonitor* monitor) noexcept
{
    timingMonitor = monitor;
}

} // namespace AudioEngine
