// ihub-plugin-state-store-test: acceptance check for T081.
//
// The central case is a set whose second slot names a plugin that is not
// installed. Loading it must:
//
//   - keep the slot in the graph with a silent placeholder,
//   - report one warnings entry naming the missing plugin,
//   - leave the first, installed slot playing normally,
//   - not crash while processing or while the store is unloaded.
//
// It also proves the capture side: a live instance produces a chunk with a
// fresh UUID, the plugin identity, its version stamp and a non-empty blob, and
// that blob restores the non-default "State Marker" into a fresh set, fresh
// engine and fresh store.

#include "project/PluginStateStore.h"
#include "project/Serializer.h"
#include "project/SetFile.h"

#include "audio/AudioEngine.h"
#include "plugins/PluginInstrument.h"

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>
#include <juce_events/juce_events.h>

#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#ifndef IHUB_TESTPLUGIN_VST3
 #define IHUB_TESTPLUGIN_VST3 "TestPlugin.vst3"
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

bool nearlyEqual (float a, float b, float tolerance = 1.0e-4f)
{
    return std::abs (a - b) <= tolerance;
}

juce::MemoryBlock makeBlob (const std::string& text)
{
    return juce::MemoryBlock (text.data(), text.size());
}

juce::PluginDescription* findTestPlugin (juce::AudioPluginFormatManager& manager,
                                         const juce::File& bundle,
                                         juce::OwnedArray<juce::PluginDescription>& types)
{
    const auto path = bundle.getFullPathName();

    for (auto* format : manager.getFormats())
        if (format->fileMightContainThisPluginType (path))
            format->findAllTypesForFile (types, path);

    for (auto* type : types)
        if (type->name.contains ("TestPlugin"))
            return type;

    return nullptr;
}

ihub::project::PluginStateChunk makePresentChunk (const juce::PluginDescription& description,
                                                  const std::string& uuid,
                                                  const juce::MemoryBlock& data)
{
    ihub::project::PluginStateChunk chunk;
    chunk.uuid = uuid;
    chunk.format = description.pluginFormatName.toStdString();
    chunk.pluginId = description.createIdentifierString().toStdString();
    chunk.name = description.name.toStdString();
    chunk.manufacturer = description.manufacturerName.toStdString();
    chunk.version = description.version.toStdString();
    chunk.data = data;
    return chunk;
}

ihub::project::PluginStateChunk makeMissingChunk (const std::string& uuid)
{
    ihub::project::PluginStateChunk chunk;
    chunk.uuid = uuid;
    chunk.format = "VST3";
    chunk.pluginId = "VST3-NeverInstalled-00000000";
    chunk.name = "Never Installed";
    chunk.manufacturer = "Nobody";
    chunk.version = "9.9.9";
    chunk.data = makeBlob ("this blob belongs to a plugin that is not installed");
    return chunk;
}

/** Builds a two-slot set: slot 0 references `present`, slot 1 `missing`. */
ihub::project::SetFile makeSet (const ihub::project::PluginStateChunk& present,
                                const ihub::project::PluginStateChunk& missing,
                                const std::string& name)
{
    using namespace ihub::project;

    SetFile set;
    set.name = name;
    set.pluginStates.push_back (present);
    set.pluginStates.push_back (missing);

    SlotConfig keys;
    keys.index = 0;
    keys.name = "Keys";
    keys.active = true;
    keys.instrument.kind = SourceKind::plugin;
    keys.instrument.id = "keys";
    keys.instrument.name = "Test Keys";
    keys.instrument.pluginState = present.uuid;

    SlotConfig lead;
    lead.index = 1;
    lead.name = "Lead";
    lead.active = true;
    lead.instrument.kind = SourceKind::plugin;
    lead.instrument.id = "lead";
    lead.instrument.name = "Missing Lead";
    lead.instrument.pluginState = missing.uuid;

    set.slots.push_back (keys);
    set.slots.push_back (lead);
    return set;
}

} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const juce::File bundle = argc > 1
        ? juce::File (juce::File (argv[1]).getFullPathName())
        : juce::File (IHUB_TESTPLUGIN_VST3);

    std::cout << "bundle: " << bundle.getFullPathName() << std::endl;
    check (bundle.isDirectory(), "TestPlugin.vst3 bundle exists");

    if (! bundle.isDirectory())
        return 1;

    juce::AudioPluginFormatManager formatManager;
    juce::addHeadlessDefaultFormatsToManager (formatManager);

    juce::OwnedArray<juce::PluginDescription> types;
    auto* found = findTestPlugin (formatManager, bundle, types);

    check (found != nullptr, "TestPlugin type is enumerated by the format manager");

    if (found == nullptr)
        return 1;

    const juce::PluginDescription description = *found;

    // --- Seed a state blob with a non-default parameter --------------------
    constexpr float seededMarker = 0.625f;
    constexpr float capturedMarker = 0.875f;

    juce::MemoryBlock seededState;
    {
        ihub::plugins::PluginInstrument seed;
        juce::String error;

        check (seed.load (formatManager, description, 48000.0, 256, error),
               "seed TestPlugin instance loads: " + error.toStdString());

        if (! seed.isLoaded())
            return 1;

        check (seed.setParameterValue ("State Marker", seededMarker),
               "the seed instance accepts the non-default State Marker");
        seed.getStateInformation (seededState);
    }

    check (seededState.getSize() > 0, "the seed instance produced a state blob");

    // --- Build a set whose second plugin is not installed ------------------
    using namespace ihub::project;

    const auto presentId = makePluginStateId();
    const auto missingId = makePluginStateId();

    check (isValidPluginStateId (presentId), "present state UUID is a safe entry name");
    check (isValidPluginStateId (missingId), "missing state UUID is a safe entry name");
    check (presentId != missingId, "the two state UUIDs are distinct");

    const auto presentChunk = makePresentChunk (description, presentId, seededState);
    const auto missingChunk = makeMissingChunk (missingId);
    auto set = makeSet (presentChunk, missingChunk, "T081 missing plugin set");

    juce::String validationError;
    check (set.validate (validationError),
           "the set validates (the chunk exists; only the plugin is absent): "
               + validationError.toStdString());

    // Round-trip through the real .ihub container: the acceptance case is a
    // *loaded* set, not only an in-memory model.
    const auto setPath = juce::File::getCurrentWorkingDirectory()
                             .getChildFile ("t081-missing-plugin.ihub");
    setPath.deleteFile();

    check (Serializer::save (set, setPath, validationError),
           "the set with the absent plugin is written: " + validationError.toStdString());

    SetFile loadedSet;
    check (Serializer::load (setPath, loadedSet, validationError),
           "the set with the absent plugin loads from disk: "
               + validationError.toStdString());
    setPath.deleteFile();

    // --- Load -------------------------------------------------------------
    AudioEngine::AudioEngine engine (2);
    engine.prepare (48000.0, 256, 2);

    PluginStateStore store;
    store.setAvailablePlugins ({ description });

    auto report = store.loadInto (loadedSet, engine);

    check (report.success, "loadInto succeeded despite the missing plugin: "
                               + report.error.toStdString());
    check (report.getNumLoaded() == 1, "one slot was bound to a live plugin");
    check (report.getNumPlaceholders() == 1, "one slot became a placeholder");
    check (report.bindings.size() == 2, "the report has one binding per plugin slot");

    check (! store.isPlaceholder (0), "slot 0 is not a placeholder");
    check (store.isPlaceholder (1), "slot 1 is a placeholder");
    check (store.getInstrument (0) != nullptr, "slot 0 owns a live PluginInstrument");
    check (store.getInstrument (1) == nullptr, "slot 1 owns no plugin instance");
    check (engine.getSlot (0).getSource() == store.getInstrument (0),
           "slot 0 is wired to the live instrument");
    check (engine.getSlot (1).getSource() != nullptr,
           "slot 1 is wired to a silent placeholder, not left dangling");

    check (report.warnings.size() == 1, "exactly one warning was reported");

    if (! report.warnings.empty())
    {
        const auto& warning = report.warnings.front();

        std::cout << "warning: " << warning.message << std::endl;

        check (warning.kind == PluginLoadWarningKind::missingPlugin,
               "the warning is classified as missing-plugin");
        check (warning.slotIndex == 1, "the warning names slot 1");
        check (warning.pluginStateId == missingId, "the warning carries the state UUID");
        check (warning.pluginId == missingChunk.pluginId, "the warning carries the plugin id");
        check (warning.pluginName == missingChunk.name, "the warning carries the plugin name");
        check (warning.message.containsIgnoreCase ("missing plugin"),
               "the warning text says the plugin is missing");
        check (warning.message.contains ("Never Installed"),
               "the warning text names the absent plugin");
    }

    auto* live = store.getInstrument (0);

    if (live == nullptr)
        return 1;

    const auto loadedMarker = live->getParameterValue ("State Marker");

    std::cout << "State Marker after load: " << loadedMarker << std::endl;

    check (nearlyEqual (loadedMarker, seededMarker),
           "the state blob restored the non-default State Marker");

    // The version stamp travels with the binding as well as the chunk.
    for (const auto& binding : report.bindings)
        if (binding.slotIndex == 0)
            check (binding.pluginVersion == description.version.toStdString(),
                   "slot 0 carries the plugin version stamp");

    // --- Render: the placeholder must be silent, not fatal -----------------
    juce::AudioBuffer<float> block (2, 256);
    juce::MidiBuffer midi;

    for (int i = 0; i < 4; ++i)
    {
        block.clear();
        engine.processBlock (block, midi);
    }

    bool finite = true;

    for (int channel = 0; channel < block.getNumChannels(); ++channel)
        for (int sample = 0; sample < block.getNumSamples(); ++sample)
            finite = finite && std::isfinite (block.getSample (channel, sample));

    check (finite, "processing with a placeholder slot produces finite audio");

    // --- Capture a live instance ------------------------------------------
    check (live->setParameterValue ("State Marker", capturedMarker),
           "the live instance accepts the State Marker before capture");

    auto captured = PluginStateStore::captureState (*live);

    std::cout << "captured uuid: " << captured.uuid
              << " plugin: " << captured.pluginId
              << " version: " << captured.version
              << " bytes: " << captured.data.getSize() << std::endl;

    check (isValidPluginStateId (captured.uuid), "the captured chunk has a safe UUID");
    check (captured.uuid != presentId, "capture assigns a fresh UUID");
    check (captured.format == description.pluginFormatName.toStdString(),
           "capture records the plugin format");
    check (captured.pluginId == description.createIdentifierString().toStdString(),
           "capture records the stable plugin identifier");
    check (captured.name == description.name.toStdString(),
           "capture records the plugin name");
    check (captured.manufacturer == description.manufacturerName.toStdString(),
           "capture records the manufacturer");
    check (! captured.version.empty()
               && captured.version == description.version.toStdString(),
           "capture records the plugin version stamp");
    check (captured.data.getSize() > 0, "capture records a non-empty state blob");

    // --- Reload the captured state into fresh objects ----------------------
    auto capturedSet = makeSet (captured, missingChunk, "T081 captured state set");

    check (capturedSet.validate (validationError),
           "the captured set validates: " + validationError.toStdString());

    AudioEngine::AudioEngine secondEngine (2);
    secondEngine.prepare (48000.0, 128, 2);

    PluginStateStore secondStore;
    secondStore.setAvailablePlugins ({ description });

    auto secondReport = secondStore.loadInto (capturedSet, secondEngine, 48000.0, 128);

    check (secondReport.success, "the captured set loads");
    check (secondReport.getNumPlaceholders() == 1,
           "the missing plugin is still a placeholder on reload");
    check (secondReport.warnings.size() == 1,
           "the reload reports the missing plugin again");

    auto* reloaded = secondStore.getInstrument (0);

    if (reloaded == nullptr)
        return 1;

    const auto reloadedMarker = reloaded->getParameterValue ("State Marker");

    std::cout << "State Marker after captured reload: " << reloadedMarker << std::endl;

    check (nearlyEqual (reloadedMarker, capturedMarker),
           "the captured blob round-trips through the store");

    // --- A structurally invalid set fails without touching the graph ------
    {
        auto dangling = set;
        dangling.slots[0].instrument.pluginState = "missing-state";
        auto* sourceBefore = engine.getSlot (0).getSource();

        const auto failed = store.loadInto (dangling, engine);

        check (! failed.success, "a dangling state reference fails the load");
        check (failed.error.contains ("missing plugin state"),
               "the structural error explains the dangling reference");
        check (engine.getSlot (0).getSource() == sourceBefore,
               "a failed load leaves the already-bound graph untouched");
    }

    // --- Unload and keep processing ---------------------------------------
    store.unload (engine);
    secondStore.unload (secondEngine);

    check (engine.getSlot (0).getSource() == nullptr,
           "unload detaches slot 0 from its instrument");
    check (engine.getSlot (1).getSource() == nullptr,
           "unload detaches slot 1 from its placeholder");
    check (secondEngine.getSlot (0).getSource() == nullptr,
           "unload detaches the second engine's instrument");

    block.clear();
    engine.processBlock (block, midi);
    secondEngine.processBlock (block, midi);
    check (true, "the engines keep rendering after unload without crashing");

    if (failures != 0)
    {
        std::cout << "FAIL: plugin state store (" << failures << " checks)" << std::endl;
        return 1;
    }

    std::cout << "PASS: plugin state store" << std::endl;
    return 0;
}
