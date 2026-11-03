#include "api/routes/RouteHelpers.h"

#include <juce_core/juce_core.h>

namespace Api
{

namespace
{
juce::var channelArrayToVar (const std::vector<AudioChannelState>& channels)
{
    juce::Array<juce::var> result;

    for (const auto& channel : channels)
    {
        auto item = Routes::makeObject();
        item.getDynamicObject()->setProperty ("index", channel.index);
        item.getDynamicObject()->setProperty ("name", juce::String (channel.name));
        item.getDynamicObject()->setProperty ("active", channel.active);
        result.add (item);
    }

    return juce::var (result);
}
}

juce::var audioStateToVar (const AudioStateSnapshot& audio)
{
    auto root = Routes::makeObject();
    auto* object = root.getDynamicObject();

    object->setProperty ("open", audio.open);
    object->setProperty ("deviceType", juce::String (audio.deviceType));
    object->setProperty ("deviceName", juce::String (audio.deviceName));

    juce::Array<juce::var> types;

    for (const auto& type : audio.deviceTypes)
        types.add (juce::String (type));

    object->setProperty ("deviceTypes", juce::var (types));

    juce::Array<juce::var> devices;

    for (const auto& device : audio.devices)
    {
        auto item = Routes::makeObject();
        item.getDynamicObject()->setProperty ("type", juce::String (device.type));
        item.getDynamicObject()->setProperty ("name", juce::String (device.name));
        item.getDynamicObject()->setProperty ("description",
                                              juce::String (device.description));
        item.getDynamicObject()->setProperty ("available", device.available);
        item.getDynamicObject()->setProperty ("reason", juce::String (device.reason));
        devices.add (item);
    }

    object->setProperty ("devices", juce::var (devices));

    juce::Array<juce::var> rawDevices;

    for (const auto& device : audio.rawDevices)
    {
        auto item = Routes::makeObject();
        item.getDynamicObject()->setProperty ("type", juce::String (device.type));
        item.getDynamicObject()->setProperty ("name", juce::String (device.name));
        item.getDynamicObject()->setProperty ("description",
                                              juce::String (device.description));
        item.getDynamicObject()->setProperty ("available", device.available);
        item.getDynamicObject()->setProperty ("reason", juce::String (device.reason));
        rawDevices.add (item);
    }

    object->setProperty ("rawDevices", juce::var (rawDevices));

    object->setProperty ("sampleRate", audio.sampleRate);
    object->setProperty ("bufferSize", audio.bufferSize);

    juce::Array<juce::var> rates;

    for (const auto rate : audio.sampleRates)
        rates.add (rate);

    object->setProperty ("sampleRates", juce::var (rates));

    juce::Array<juce::var> buffers;

    for (const auto size : audio.bufferSizes)
        buffers.add (size);

    object->setProperty ("bufferSizes", juce::var (buffers));

    object->setProperty ("inputChannels", channelArrayToVar (audio.inputChannels));
    object->setProperty ("outputChannels", channelArrayToVar (audio.outputChannels));

    auto latency = Routes::makeObject();
    latency.getDynamicObject()->setProperty ("bufferMs", audio.latency.bufferMs);
    latency.getDynamicObject()->setProperty ("deviceMs", audio.latency.deviceMs);
    latency.getDynamicObject()->setProperty ("chainMs", audio.latency.chainMs);
    latency.getDynamicObject()->setProperty ("totalMs", audio.latency.totalMs);
    object->setProperty ("latency", latency);

    object->setProperty ("xruns", static_cast<juce::int64> (audio.xruns));
    object->setProperty ("deviceXruns", static_cast<juce::int64> (audio.deviceXruns));
    object->setProperty ("callbackOverruns", static_cast<juce::int64> (audio.callbackOverruns));
    object->setProperty ("callbacks", static_cast<juce::int64> (audio.callbacks));
    object->setProperty ("loadPercent", audio.loadPercent);

    auto restore = Routes::makeObject();
    restore.getDynamicObject()->setProperty ("usedStoredDevice",
                                             audio.restore.usedStoredDevice);
    restore.getDynamicObject()->setProperty ("fallback", audio.restore.fallback);
    restore.getDynamicObject()->setProperty ("reason",
                                             juce::String (audio.restore.reason));
    object->setProperty ("restore", restore);

    return root;
}

juce::var audioMeasurementToVar (const AudioMeasurementState& measurement)
{
    auto root = Routes::makeObject();
    auto* object = root.getDynamicObject();

    object->setProperty ("valid", measurement.valid);
    object->setProperty ("roundTripSamples", measurement.roundTripSamples);
    object->setProperty ("roundTripMs", measurement.roundTripMs);
    object->setProperty ("expectedMs", measurement.expectedMs);
    object->setProperty ("confidence", static_cast<double> (measurement.confidence));
    object->setProperty ("sampleRate", measurement.sampleRate);
    object->setProperty ("bufferSize", measurement.bufferSize);
    object->setProperty ("error", juce::String (measurement.error));

    return root;
}

}
