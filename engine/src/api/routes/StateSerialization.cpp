#include "api/routes/RouteHelpers.h"

#include <juce_core/juce_core.h>

namespace Api
{

juce::var stateToVar (const StateSnapshot& state)
{
    auto root = Routes::makeObject();
    auto* object = root.getDynamicObject();

    object->setProperty ("schemaVersion", state.schemaVersion);
    object->setProperty ("revision", static_cast<juce::int64> (state.revision));

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

    auto project = Routes::makeObject();
    auto* projectObject = project.getDynamicObject();
    projectObject->setProperty ("path", juce::String (state.project.path));
    projectObject->setProperty ("name", juce::String (state.project.name));
    projectObject->setProperty ("dirty", state.project.dirty);
    projectObject->setProperty ("saved", state.project.saved);
    projectObject->setProperty ("error", juce::String (state.project.error));
    object->setProperty ("project", project);

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

}
