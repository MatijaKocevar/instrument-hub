// ihub-patch-persistence-test: acceptance check for T119.
//
// The check has two halves:
//
//   1. `shared/set.yaml` carries every patch's persistent state: instrument
//      descriptor, volume/pan/mute/solo, the resolved colour index (family
//      default plus patch override) and the ordered FX chain. All 64 patches
//      resolve, including the family-default and override colour cases.
//
//   2. A 64-slot `.ihub` set built from that document round-trips through the
//      real container. Exactly one patch has been loaded/edited, so exactly
//      one plugin state blob is stored; the other 63 patches are descriptors
//      only. Their mixer state, colour index and FX chain survive byte for
//      byte, and `Serializer::load()` leaves them cold.

#include "controller/Mapping.h"
#include "project/Serializer.h"
#include "project/SetFile.h"

#include "audio/AudioEngine.h"

#include <juce_events/juce_events.h>

#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#ifndef IHUB_SET_YAML
 #define IHUB_SET_YAML "shared/set.yaml"
#endif

namespace
{

int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

bool nearlyEqual (float a, float b, float tolerance = 1.0e-5f)
{
    return std::abs (a - b) <= tolerance;
}

std::string readEntry (juce::ZipFile& zip, const juce::String& name)
{
    const auto* entry = zip.getEntry (name);

    if (entry == nullptr || entry->uncompressedSize < 0)
        return {};

    std::unique_ptr<juce::InputStream> stream (zip.createStreamForEntry (*entry));

    if (stream == nullptr)
        return {};

    juce::MemoryBlock data;
    data.setSize (static_cast<std::size_t> (entry->uncompressedSize));

    if (entry->uncompressedSize > 0)
        stream->read (data.getData(), static_cast<int> (entry->uncompressedSize));

    if (data.getSize() == 0)
        return {};

    return std::string (static_cast<const char*> (data.getData()), data.getSize());
}

ihub::project::SourceKind sourceKindFor (const std::string& kind)
{
    using ihub::project::SourceKind;

    if (kind == "plugin")
        return SourceKind::plugin;

    if (kind == "sampler")
        return SourceKind::sampler;

    if (kind == "audio-input")
        return SourceKind::audioInput;

    return SourceKind::empty;
}

/** Builds the 64-slot document the container would persist. */
ihub::project::SetFile buildSet (const Controller::SetMapping& document,
                                 const std::string& mappingYaml,
                                 const std::string& hotStateId)
{
    using namespace ihub::project;

    SetFile set;
    set.name = document.name.empty() ? "T119 64-patch set" : document.name;
    set.mappingYaml = mappingYaml;

    for (int pad = 0; pad < Controller::SetMapping::maximumPads; ++pad)
    {
        const auto* binding = document.padBinding (pad);

        if (binding == nullptr)
            continue;

        const auto* patch = document.instrumentFor (*binding);

        if (patch == nullptr)
            continue;

        SlotConfig slot;
        slot.index = patch->slot;
        slot.name = patch->name;
        slot.setVolumeDb (patch->volumeDb);
        slot.pan = patch->pan;
        slot.muted = patch->muted;
        slot.solo = patch->solo;
        slot.colorIndex = document.colorIndexFor (*binding);
        slot.active = patch->slot == 0;

        slot.instrument.kind = sourceKindFor (patch->kind);
        slot.instrument.id = patch->id;
        slot.instrument.name = patch->name;
        slot.instrument.pluginFormat = patch->pluginFormat;
        slot.instrument.pluginId = patch->pluginId;
        slot.instrument.pluginName = patch->pluginName;
        slot.instrument.pluginManufacturer = patch->pluginManufacturer;
        slot.instrument.pluginVersion = patch->pluginVersion;
        slot.instrument.samplePath = patch->samplePath;
        slot.instrument.pluginState = patch->pluginState;
        slot.instrument.gain = patch->volumeDb <= -60.0f
            ? 0.0f
            : std::pow (10.0f, patch->volumeDb / 20.0f);

        for (const auto& patchFx : patch->fxChain)
        {
            FxConfig fx;
            fx.kind = patchFx.kind == "plugin" ? FxKind::plugin : FxKind::builtin;
            fx.id = patchFx.id;
            fx.name = patchFx.name;
            fx.pluginState = patchFx.pluginState;
            fx.bypassed = patchFx.bypassed;
            fx.parameters = patchFx.parameters;
            slot.fxChain.push_back (std::move (fx));
        }

        // The shipped set is made of empty (unassigned) descriptors. For the
        // container round-trip every patch becomes a plugin descriptor; slot 0
        // is the one patch that has been loaded, so it is the only one with a
        // blob.
        if (patch->slot == 0)
        {
            slot.instrument.kind = SourceKind::plugin;
            slot.instrument.pluginId = "com.instrumenthub.t119.hot";
            slot.instrument.pluginName = patch->name;
            slot.instrument.pluginState = hotStateId;
        }
        else if (slot.instrument.kind == SourceKind::empty)
        {
            slot.instrument.kind = SourceKind::plugin;
            slot.instrument.pluginId = "com.instrumenthub.t119.cold." + patch->id;
            slot.instrument.pluginName = patch->name;
            slot.instrument.pluginState.clear();
        }

        set.slots.push_back (std::move (slot));
    }

    return set;
}

} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    //==========================================================================
    // 1. set.yaml carries the full patch state
    //==========================================================================
    Controller::SetMapping document;
    Controller::MappingError mappingError;

    const bool documentLoaded = Controller::loadSetMappingFile (IHUB_SET_YAML,
                                                               document,
                                                               mappingError);

    check (documentLoaded,
           std::string ("shared/set.yaml loads")
               + (documentLoaded
                      ? std::string()
                      : " (" + mappingError.message + ":"
                            + std::to_string (mappingError.line) + ")"));

    if (! documentLoaded)
        return 1;

    check (document.countPatches() == 64,
           "set.yaml declares 64 patches (got "
               + std::to_string (document.countPatches()) + ")");

    bool everyPatchHasDescriptor = true;
    bool everyFamilyHasColour = true;

    for (const auto& [familyId, family] : document.families)
    {
        (void) familyId;

        if (family.colorIndex < 0
            || family.colorIndex > Controller::SetMapping::maximumColourIndex)
            everyFamilyHasColour = false;

        for (const auto& patch : family.instruments)
        {
            everyPatchHasDescriptor = everyPatchHasDescriptor
                && ! patch.id.empty()
                && ! patch.name.empty()
                && patch.slot >= 0
                && patch.slot <= Controller::SetMapping::maximumSlot
                && patch.kind == "empty"
                && patch.resolvedColorIndex >= 0
                && patch.resolvedColorIndex <= Controller::SetMapping::maximumColourIndex;
        }
    }

    check (everyFamilyHasColour, "every family declares a family colour default");
    check (everyPatchHasDescriptor,
           "every patch carries its instrument descriptor and resolved colour");

    const auto* pianoBinding = document.padBinding (0);
    const auto* piano = pianoBinding != nullptr ? document.instrumentFor (*pianoBinding) : nullptr;

    check (piano != nullptr, "keys/piano resolves from pad 0");

    if (piano != nullptr && pianoBinding != nullptr)
    {
        check (piano->colorIndex == 1,
               "keys/piano carries a patch colour override");
        check (document.families.at ("keys").colorIndex == 0,
               "the keys family declares the default colour");
        check (piano->resolvedColorIndex == 1,
               "the patch override wins over the family default");
        check (document.colorIndexFor (*pianoBinding) == 1,
               "the mapping reports the resolved patch colour");
        check (nearlyEqual (piano->volumeDb, -2.5f),
               "keys/piano persists its volume");
        check (nearlyEqual (piano->pan, -0.15f),
               "keys/piano persists its pan");
        check (! piano->muted && ! piano->solo,
               "keys/piano persists its mute/solo state");
        check (piano->fxChain.size() == 1,
               "keys/piano persists its FX chain");
        check (piano->fxChain.size() == 1
                   && piano->fxChain.front().id == "eq"
                   && nearlyEqual (piano->fxChain.front().parameters.at ("low"), 2.0f)
                   && nearlyEqual (piano->fxChain.front().parameters.at ("high"), -1.0f),
               "keys/piano's FX parameters survive parsing");
    }

    const auto* stringsBinding = document.padBinding (1);
    const auto* strings = stringsBinding != nullptr
                              ? document.instrumentFor (*stringsBinding)
                              : nullptr;

    check (strings != nullptr && strings->colorIndex == -1
               && strings->resolvedColorIndex == 0,
           "a patch without an override inherits the family colour");

    //==========================================================================
    // 2. A 64-patch .ihub round-trips with one hot patch
    //==========================================================================
    const juce::File mappingFile (IHUB_SET_YAML);
    const auto mappingYaml = mappingFile.loadFileAsString().toStdString();

    check (! mappingYaml.empty(), "the mapping text is available");

    const auto hotStateId = ihub::project::makePluginStateId();

    ihub::project::PluginStateChunk hotState;
    hotState.uuid = hotStateId;
    hotState.format = "VST3";
    hotState.pluginId = "com.instrumenthub.t119.hot";
    hotState.name = "T119 Hot Patch";
    hotState.manufacturer = "Instrument Hub";
    hotState.version = "1.0.0";

    const std::string hotBytes = "T119-loaded-patch-state";
    hotState.data = juce::MemoryBlock (hotBytes.data(), hotBytes.size());

    auto original = buildSet (document, mappingYaml, hotStateId);
    original.pluginStates.push_back (hotState);

    juce::String error;
    check (original.slots.size() == 64,
           "the built set has 64 patches (got "
               + std::to_string (original.slots.size()) + ")");
    check (original.pluginStates.size() == 1,
           "only the loaded patch owns a state blob");
    check (original.validate (error),
           "the 64-patch set validates: " + error.toStdString());

    const juce::File setPath =
        juce::File::getCurrentWorkingDirectory().getChildFile ("t119-patches.ihub");
    setPath.deleteFile();

    check (ihub::project::Serializer::save (original, setPath, error),
           "the 64-patch set is written: " + error.toStdString());

    {
        juce::ZipFile zip (setPath);

        check (zip.getNumEntries() == 3,
               "the container has set.json, mapping.yaml and one state blob (got "
                   + std::to_string (zip.getNumEntries()) + ")");
        check (zip.getEntry (juce::String (
                   ihub::project::pluginStateEntryPath (hotStateId))) != nullptr,
               "the loaded patch's blob is stored");
        check (readEntry (zip, "mapping.yaml") == mappingYaml,
               "mapping.yaml is stored byte for byte");

        const auto manifest = readEntry (zip, "set.json");
        const auto root = juce::JSON::parse (juce::String (manifest));
        const auto* object = root.getDynamicObject();
        check (object != nullptr, "set.json is a JSON object");

        if (object != nullptr)
        {
            const auto slotsValue = object->getProperty ("slots");
            const auto* slots = slotsValue.getArray();

            check (slots != nullptr && slots->size() == 64,
                   "set.json carries all 64 patches");

            if (slots != nullptr && slots->size() == 64)
            {
                const auto* first = slots->getReference (0).getDynamicObject();
                const auto* second = slots->getReference (1).getDynamicObject();

                check (first != nullptr
                           && static_cast<int> (first->getProperty ("colorIndex")) == 1,
                       "the patch colour override is in set.json");
                check (first != nullptr
                           && nearlyEqual (static_cast<float> (first->getProperty ("pan")),
                                          -0.15f),
                       "the patch pan is in set.json");
                check (first != nullptr
                           && static_cast<bool> (first->getProperty ("solo")) == false,
                       "the patch solo flag is in set.json");
                check (second != nullptr
                           && static_cast<int> (second->getProperty ("colorIndex")) == 0,
                       "the inherited family colour is persisted per patch");
            }
        }
    }

    ihub::project::SetFile loaded;
    check (ihub::project::Serializer::load (setPath, loaded, error),
           "the 64-patch set loads: " + error.toStdString());
    check (loaded == original,
           "all 64 patches round-trip deep-equal");
    check (loaded.pluginStates.size() == 1,
           "the round-trip stores exactly one state blob");

    int coldPatches = 0;
    int hotPatches = 0;

    for (const auto& slot : loaded.slots)
    {
        if (slot.hasPluginState())
            ++hotPatches;
        else
            ++coldPatches;
    }

    check (hotPatches == 1, "exactly one patch restores plugin state");
    check (coldPatches == 63, "the other 63 patches stay descriptors only");

    const auto* restored = loaded.findPluginState (hotStateId);
    check (restored != nullptr,
           "the loaded patch's state chunk is found by UUID");
    check (restored != nullptr
               && std::string (static_cast<const char*> (restored->data.getData()),
                               restored->data.getSize()) == hotBytes,
           "the loaded patch's blob bytes survive the round-trip");
    check (loaded.slots[0].instrument.pluginId == "com.instrumenthub.t119.hot",
           "the loaded patch keeps its plugin descriptor");
    check (loaded.slots[1].instrument.pluginId
               == "com.instrumenthub.t119.cold." + loaded.slots[1].instrument.id,
           "a cold patch keeps its plugin descriptor without a blob");
    check (loaded.slots[0].fxChain.size() == 1
               && loaded.slots[0].fxChain.front().id == "eq",
           "the loaded patch keeps its FX chain");
    check (nearlyEqual (loaded.slots[0].volumeDb(), -2.5f, 0.001f),
           "the loaded patch keeps its volume");
    check (nearlyEqual (loaded.slots[0].pan, -0.15f),
           "the loaded patch keeps its pan");
    check (loaded.slots[0].colorIndex == 1,
           "the loaded patch keeps its resolved colour");

    // --- Apply the descriptors without instantiating anything --------------
    AudioEngine::AudioEngine engine (64);
    engine.prepare (48000.0, 128, 2);
    check (loaded.applyTo (engine, error),
           "the 64 descriptors apply to a 64-slot engine: " + error.toStdString());

    setPath.deleteFile();

    if (failures != 0)
    {
        std::cout << "FAIL: patch persistence (" << failures << " checks)" << std::endl;
        return 1;
    }

    std::cout << "PASS: patch persistence" << std::endl;
    return 0;
}
