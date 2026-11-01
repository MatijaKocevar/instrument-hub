#include "api/routes/RouteHelpers.h"

#include <juce_core/juce_core.h>

namespace Api
{

juce::var stateToVar (const StateSnapshot& state)
{
    auto root = Routes::makeObject();
    auto* object = root.getDynamicObject();

    object->setProperty ("schemaVersion", state.schemaVersion);

    auto application = Routes::makeObject();
    application.getDynamicObject()->setProperty ("name", juce::String (state.applicationName));
    application.getDynamicObject()->setProperty ("version",
                                                 juce::String (state.applicationVersion));
    object->setProperty ("application", application);

    object->setProperty ("armedSlot", state.armedSlot);
    object->setProperty ("uptimeMs", state.uptimeMs);

    juce::Array<juce::var> slots;

    for (const auto& slot : state.slots)
    {
        auto slotVar = Routes::makeObject();
        slotVar.getDynamicObject()->setProperty ("index", slot.index);
        slotVar.getDynamicObject()->setProperty ("name", juce::String (slot.name));
        slotVar.getDynamicObject()->setProperty ("armed", slot.armed);
        slotVar.getDynamicObject()->setProperty ("state", juce::String (slot.state));
        slotVar.getDynamicObject()->setProperty ("warmState", juce::String (slot.warmState));
        slotVar.getDynamicObject()->setProperty ("warm", slot.warm);
        slotVar.getDynamicObject()->setProperty ("family", juce::String (slot.family));
        slotVar.getDynamicObject()->setProperty ("familyName", juce::String (slot.familyName));
        slotVar.getDynamicObject()->setProperty ("patch", juce::String (slot.patch));
        slotVar.getDynamicObject()->setProperty ("patchName", juce::String (slot.patchName));
        slotVar.getDynamicObject()->setProperty ("gain", static_cast<double> (slot.gain));
        slotVar.getDynamicObject()->setProperty ("latencySamples", slot.latencySamples);
        slotVar.getDynamicObject()->setProperty ("latencyMs", slot.latencyMs);
        slots.add (slotVar);
    }

    object->setProperty ("slots", juce::var (slots));

    juce::Array<juce::var> families;

    for (const auto& family : state.families)
    {
        auto familyVar = Routes::makeObject();
        familyVar.getDynamicObject()->setProperty ("id", juce::String (family.id));
        familyVar.getDynamicObject()->setProperty ("name", juce::String (family.name));

        juce::Array<juce::var> patches;

        for (const auto& patch : family.patches)
        {
            auto patchVar = Routes::makeObject();
            patchVar.getDynamicObject()->setProperty ("slot", patch.slot);
            patchVar.getDynamicObject()->setProperty ("id", juce::String (patch.id));
            patchVar.getDynamicObject()->setProperty ("name", juce::String (patch.name));
            patchVar.getDynamicObject()->setProperty ("family", juce::String (patch.family));
            patchVar.getDynamicObject()->setProperty ("familyName",
                                                      juce::String (patch.familyName));

            auto instrument = Routes::makeObject();
            instrument.getDynamicObject()->setProperty (
                "id", juce::String (patch.instrument.id));
            instrument.getDynamicObject()->setProperty (
                "name", juce::String (patch.instrument.name));
            instrument.getDynamicObject()->setProperty (
                "kind", juce::String (patch.instrument.kind));
            instrument.getDynamicObject()->setProperty (
                "pluginFormat", juce::String (patch.instrument.pluginFormat));
            instrument.getDynamicObject()->setProperty (
                "pluginId", juce::String (patch.instrument.pluginId));
            instrument.getDynamicObject()->setProperty (
                "pluginName", juce::String (patch.instrument.pluginName));
            instrument.getDynamicObject()->setProperty (
                "pluginManufacturer", juce::String (patch.instrument.pluginManufacturer));
            instrument.getDynamicObject()->setProperty (
                "pluginVersion", juce::String (patch.instrument.pluginVersion));
            instrument.getDynamicObject()->setProperty (
                "samplePath", juce::String (patch.instrument.samplePath));
            patchVar.getDynamicObject()->setProperty ("instrument", instrument);

            patchVar.getDynamicObject()->setProperty ("volumeDb", patch.volumeDb);
            patchVar.getDynamicObject()->setProperty ("pan", patch.pan);
            patchVar.getDynamicObject()->setProperty ("mute", patch.mute);
            patchVar.getDynamicObject()->setProperty ("solo", patch.solo);
            patchVar.getDynamicObject()->setProperty ("colorIndex", patch.colorIndex);
            patchVar.getDynamicObject()->setProperty ("warmState",
                                                      juce::String (patch.warmState));
            patchVar.getDynamicObject()->setProperty ("warm", patch.warm);
            patchVar.getDynamicObject()->setProperty ("editorOpen", patch.editorOpen);
            patchVar.getDynamicObject()->setProperty ("latencySamples",
                                                      patch.latencySamples);
            patchVar.getDynamicObject()->setProperty ("latencyMs", patch.latencyMs);
            patches.add (patchVar);
        }

        familyVar.getDynamicObject()->setProperty ("patches", juce::var (patches));
        families.add (familyVar);
    }

    object->setProperty ("families", juce::var (families));
    object->setProperty ("settings", settingsToVar (state.settings));
    object->setProperty ("transport", transportToVar (state.transport));

    return root;
}

juce::var settingsToVar (const SettingsState& settings)
{
    auto root = Routes::makeObject();
    auto* object = root.getDynamicObject();

    object->setProperty ("masterGain", settings.masterGain);
    object->setProperty ("inputGain", settings.inputGain);
    object->setProperty ("midiChannel", settings.midiChannel);
    object->setProperty ("metronome", settings.metronome);

    return root;
}

juce::var transportToVar (const TransportState& transport)
{
    auto root = Routes::makeObject();
    auto* object = root.getDynamicObject();

    object->setProperty ("playing", transport.playing);
    object->setProperty ("bpm", transport.bpm);
    object->setProperty ("ppq", transport.ppq);
    object->setProperty ("positionSamples", static_cast<juce::int64> (transport.positionSamples));

    return root;
}

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
} // namespace

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
        devices.add (item);
    }

    object->setProperty ("devices", juce::var (devices));

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

juce::var pairingToVar (const PairingSnapshot& pairing)
{
    auto root = Routes::makeObject();
    auto* object = root.getDynamicObject();

    object->setProperty ("lanEnabled", pairing.lanEnabled);
    object->setProperty ("address", juce::String (pairing.address));
    object->setProperty ("port", pairing.port);
    object->setProperty ("code", juce::String (pairing.code));
    object->setProperty ("codeExpiresInMs", pairing.codeExpiresInMs);
    object->setProperty ("payload", juce::String (pairing.payload));
    object->setProperty ("pairedClients", pairing.pairedClients);

    return root;
}

juce::var meterFrameToVar (const MeterFrame& frame)
{
    auto root = Routes::makeObject();
    auto* object = root.getDynamicObject();

    object->setProperty ("type", "meters");
    object->setProperty ("sequence", static_cast<juce::int64> (frame.sequence));
    object->setProperty ("timestampMs", frame.timestampMs);

    auto master = Routes::makeObject();
    master.getDynamicObject()->setProperty ("peak", static_cast<double> (frame.masterPeak));
    object->setProperty ("master", master);

    juce::Array<juce::var> slots;

    for (const auto& slot : frame.slots)
    {
        auto slotVar = Routes::makeObject();
        slotVar.getDynamicObject()->setProperty ("slot", slot.slot);
        slotVar.getDynamicObject()->setProperty ("peak", static_cast<double> (slot.peak));
        slotVar.getDynamicObject()->setProperty ("gain", static_cast<double> (slot.gain));
        slotVar.getDynamicObject()->setProperty ("armed", slot.armed);
        slots.add (slotVar);
    }

    object->setProperty ("slots", juce::var (slots));

    return root;
}

juce::var midiStateToVar (const MidiStateSnapshot& midi)
{
    auto root = Routes::makeObject();
    auto* object = root.getDynamicObject();

    juce::Array<juce::var> inputs;

    for (const auto& input : midi.inputs)
    {
        auto item = Routes::makeObject();
        auto* itemObject = item.getDynamicObject();

        itemObject->setProperty ("id", juce::String (input.id));
        itemObject->setProperty ("name", juce::String (input.name));
        itemObject->setProperty ("role", juce::String (input.role));
        itemObject->setProperty ("port", juce::String (input.port));
        itemObject->setProperty ("enabled", input.enabled);
        itemObject->setProperty ("open", input.open);
        itemObject->setProperty ("events", static_cast<juce::int64> (input.events));
        itemObject->setProperty ("lastEvent", juce::String (input.lastEvent));
        inputs.add (item);
    }

    object->setProperty ("inputs", juce::var (inputs));
    object->setProperty ("inputCount", static_cast<int> (midi.inputs.size()));

    const auto portToVar = [] (const MidiPortState& port)
    {
        auto item = Routes::makeObject();
        auto* itemObject = item.getDynamicObject();

        itemObject->setProperty ("name", juce::String (port.name));
        itemObject->setProperty ("port", juce::String (port.port));
        itemObject->setProperty ("connected", port.connected);
        itemObject->setProperty ("handshake", port.handshake);
        itemObject->setProperty ("inputName", juce::String (port.inputName));
        itemObject->setProperty ("outputName", juce::String (port.outputName));
        itemObject->setProperty ("error", juce::String (port.error));
        return item;
    };

    object->setProperty ("launchpad", portToVar (midi.launchpad));
    object->setProperty ("keystation", portToVar (midi.keystation));

    auto padTest = Routes::makeObject();
    padTest.getDynamicObject()->setProperty ("active", midi.padTest.active);
    padTest.getDynamicObject()->setProperty ("lit", midi.padTest.lit);
    padTest.getDynamicObject()->setProperty ("durationMs", midi.padTest.durationMs);
    object->setProperty ("padTest", padTest);

    object->setProperty ("error", juce::String (midi.error));

    return root;
}

} // namespace Api
