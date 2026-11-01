#include "project/SetFile.h"
#include "project/Serializer.h"

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

bool nearlyEqual (float a, float b)
{
    return std::abs (a - b) <= 1.0e-6f;
}

juce::MemoryBlock makeBlob (const std::string& text)
{
    return juce::MemoryBlock (text.data(), text.size());
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

ihub::project::SetFile makeAcceptanceSet (const std::string& mappingYaml,
                                          const std::string& pluginStateId1,
                                          const std::string& pluginStateId2)
{
    using namespace ihub::project;

    SetFile set;
    set.name = "T080 Acceptance Set";
    set.mappingYaml = mappingYaml;
    set.transport.bpm = 123.5;
    set.transport.positionPpq = 7.25;
    set.transport.playing = true;

    PluginStateChunk pianoState;
    pianoState.uuid = pluginStateId1;
    pianoState.format = "VST3";
    pianoState.pluginId = "com.instrumenthub.testpiano";
    pianoState.name = "Test Piano";
    pianoState.manufacturer = "Instrument Hub";
    pianoState.version = "1.2.3";
    pianoState.data = makeBlob ("piano-state-payload-0123456789");
    set.pluginStates.push_back (pianoState);

    PluginStateChunk stringsState;
    stringsState.uuid = pluginStateId2;
    stringsState.format = "VST3";
    stringsState.pluginId = "com.instrumenthub.teststrings";
    stringsState.name = "Test Strings";
    stringsState.manufacturer = "Instrument Hub";
    stringsState.version = "4.5.6";
    stringsState.data = makeBlob (std::string ("strings\0state\0with\0nulls", 25));
    set.pluginStates.push_back (stringsState);

    SlotConfig piano;
    piano.index = 0;
    piano.name = "Piano";
    piano.gain = 0.75f;
    piano.pan = -0.25f;
    piano.colorIndex = 3;
    piano.solo = true;
    piano.active = true;
    piano.instrument.kind = SourceKind::plugin;
    piano.instrument.id = "piano";
    piano.instrument.name = "Grand Piano";
    piano.instrument.pluginState = pluginStateId1;
    piano.instrument.inputChannel = 0;
    piano.instrument.gain = 0.9f;
    piano.instrument.directMonitor = false;

    FxConfig eq;
    eq.kind = FxKind::builtin;
    eq.id = "eq";
    eq.name = "EQ";
    eq.bypassed = false;
    eq.parameters["low"] = 0.25f;
    eq.parameters["high"] = -0.125f;
    piano.fxChain.push_back (eq);
    set.slots.push_back (piano);

    SlotConfig strings;
    strings.index = 1;
    strings.name = "Strings";
    strings.gain = 0.6f;
    strings.active = true;
    strings.instrument.kind = SourceKind::plugin;
    strings.instrument.id = "strings";
    strings.instrument.name = "Warm Strings";
    strings.instrument.pluginState = pluginStateId2;
    strings.instrument.gain = 0.8f;

    FxConfig hostedFx;
    hostedFx.kind = FxKind::plugin;
    hostedFx.id = "test-strings-fx";
    hostedFx.name = "Strings FX";
    hostedFx.pluginState = pluginStateId2;
    hostedFx.bypassed = true;
    hostedFx.parameters["mix"] = 0.5f;
    strings.fxChain.push_back (hostedFx);
    set.slots.push_back (strings);

    SlotConfig drums;
    drums.index = 2;
    drums.name = "Drums";
    drums.gain = 1.0f;
    drums.active = false;
    drums.instrument.kind = SourceKind::sampler;
    drums.instrument.id = "kit";
    drums.instrument.name = "Acoustic Kit";

    FxConfig compressor;
    compressor.kind = FxKind::builtin;
    compressor.id = "compressor";
    compressor.name = "Compressor";
    compressor.parameters["ratio"] = 4.0f;
    compressor.parameters["threshold"] = -18.0f;
    drums.fxChain.push_back (compressor);
    set.slots.push_back (drums);

    SlotConfig input;
    input.index = 3;
    input.name = "Input";
    input.gain = 0.5f;
    input.active = false;
    input.muted = true;
    input.instrument.kind = SourceKind::audioInput;
    input.instrument.id = "input";
    input.instrument.name = "Audio Input";
    input.instrument.inputChannel = 1;
    input.instrument.directMonitor = true;
    set.slots.push_back (input);

    return set;
}

}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const juce::File directory = juce::File::getCurrentWorkingDirectory();
    const juce::File setPath = directory.getChildFile ("t080-acceptance.ihub");
    const juce::File reloadedPath = directory.getChildFile ("t080-acceptance-reloaded.ihub");
    const juce::File badPath = directory.getChildFile ("t080-invalid.ihub");

    setPath.deleteFile();
    reloadedPath.deleteFile();
    badPath.deleteFile();

    const juce::File mappingFile (IHUB_SET_YAML);
    check (mappingFile.existsAsFile(), "shared/set.yaml is available");

    if (! mappingFile.existsAsFile())
        return 1;

    const auto mappingYaml = mappingFile.loadFileAsString().toStdString();
    check (! mappingYaml.empty(), "mapping.yaml is non-empty");

    const auto stateId1 = ihub::project::makePluginStateId();
    const auto stateId2 = ihub::project::makePluginStateId();

    check (ihub::project::isValidPluginStateId (stateId1),
           "generated plugin state id is a safe entry name");
    check (stateId1 != stateId2, "generated plugin state ids are distinct");

    auto original = makeAcceptanceSet (mappingYaml, stateId1, stateId2);

    juce::String error;
    check (original.validate (error), "the built set validates: " + error.toStdString());
    check (original.slots.size() == 4, "the set has four slots");
    check (original.pluginStates.size() == 2, "the set has two plugin state chunks");

    check (ihub::project::Serializer::save (original, setPath, error),
           "Serializer::save writes the container: " + error.toStdString());
    check (setPath.existsAsFile(), "the .ihub file exists");
    check (setPath.getSize() > 0, "the .ihub file is not empty");

    {
        juce::ZipFile zip (setPath);

        check (zip.getNumEntries() == 4, "the archive has exactly four entries");
        check (zip.getEntry ("set.json") != nullptr, "the archive contains set.json");
        check (zip.getEntry ("mapping.yaml") != nullptr, "the archive contains mapping.yaml");

        const auto statePath1 = ihub::project::pluginStateEntryPath (stateId1);
        const auto statePath2 = ihub::project::pluginStateEntryPath (stateId2);

        check (zip.getEntry (juce::String (statePath1)) != nullptr,
               "the archive contains plugin-state/" + stateId1 + ".bin");
        check (zip.getEntry (juce::String (statePath2)) != nullptr,
               "the archive contains plugin-state/" + stateId2 + ".bin");

        const auto manifest = readEntry (zip, "set.json");
        check (juce::JSON::parse (juce::String (manifest)).isObject(),
               "set.json is valid JSON");
        check (manifest.find ("piano-state-payload") == std::string::npos,
               "plugin bytes are not inlined in set.json");
        const std::string nullNeedle ("strings\0state", 13);
        check (manifest.find (nullNeedle) == std::string::npos,
               "binary plugin bytes are not inlined in set.json");

        const auto storedMapping = readEntry (zip, "mapping.yaml");
        check (storedMapping == mappingYaml,
               "mapping.yaml is stored byte for byte");
    }

    ihub::project::SetFile loaded;
    check (ihub::project::Serializer::load (setPath, loaded, error),
           "Serializer::load reads the container: " + error.toStdString());

    check (loaded == original, "the reloaded set is deeply equal to the original");
    check (loaded.schemaVersion == ihub::project::currentSetSchemaVersion,
           "the schema version survives the round-trip");
    check (loaded.mappingYaml == mappingYaml,
           "the mapping survives the round-trip");
    check (nearlyEqual (loaded.slots[0].pan, -0.25f)
               && loaded.slots[0].colorIndex == 3
               && loaded.slots[0].solo,
           "the reloaded patch keeps pan, colour index and solo");

    AudioEngine::AudioEngine freshEngine (4);
    freshEngine.prepare (48000.0, 128, 2);

    check (freshEngine.getNumSlots() == 4, "the fresh engine has four slots");
    check (loaded.applyTo (freshEngine, error),
           "the loaded set applies to a fresh engine: " + error.toStdString());

    for (const auto& slot : loaded.slots)
    {
        auto& live = freshEngine.getSlot (slot.index);
        const auto expectedGain = slot.muted ? 0.0f : slot.gain;

        check (nearlyEqual (live.getGain(), expectedGain),
               "fresh engine slot " + std::to_string (slot.index)
                   + " has the loaded gain");
        check (live.isActivated() == slot.active,
               "fresh engine slot " + std::to_string (slot.index)
                   + " has the loaded activation state");
    }

    check (ihub::project::Serializer::save (loaded, reloadedPath, error),
           "the reloaded set can be written again: " + error.toStdString());

    ihub::project::SetFile secondLoad;
    check (ihub::project::Serializer::load (reloadedPath, secondLoad, error),
           "the second container loads: " + error.toStdString());
    check (secondLoad == original, "the second round-trip is still deeply equal");

    {
        auto unsafe = original;
        unsafe.pluginStates[0].uuid = "../escape";
        juce::String validationError;
        check (! unsafe.validate (validationError),
               "an unsafe plugin state id is rejected");
        check (! ihub::project::Serializer::save (unsafe, badPath, validationError),
               "an unsafe set is not written");
        check (! badPath.existsAsFile(), "the rejected set left no file behind");
    }

    {
        auto duplicate = original;
        duplicate.pluginStates[1].uuid = duplicate.pluginStates[0].uuid;
        juce::String validationError;
        check (! duplicate.validate (validationError),
               "duplicate plugin state ids are rejected");
    }

    {
        auto dangling = original;
        dangling.slots[0].instrument.pluginState = "missing-state";
        juce::String validationError;
        check (! dangling.validate (validationError),
               "a dangling plugin state reference is rejected");
    }

    {
        auto cold = original;
        cold.slots[0].instrument.pluginState.clear();
        cold.slots[0].instrument.pluginId = "com.instrumenthub.cold";
        cold.pluginStates.erase (cold.pluginStates.begin());
        juce::String validationError;
        check (cold.validate (validationError),
               "a cold plugin descriptor without a state blob validates: "
                   + validationError.toStdString());
        check (! cold.slots[0].hasPluginState(),
               "the cold patch reports no plugin state");
    }

    {
        auto invalidGain = original;
        invalidGain.slots[0].gain = 8.0f;
        juce::String validationError;
        check (! invalidGain.validate (validationError),
               "an out-of-range slot gain is rejected");
    }

    {
        juce::String loadError;
        ihub::project::SetFile untouched;
        untouched.name = "sentinel";

        check (! ihub::project::Serializer::load (badPath, untouched, loadError),
               "a missing file is rejected");
        check (untouched.name == "sentinel",
               "a failed load leaves the destination untouched");
    }

    {
        const auto corruptPath = directory.getChildFile ("t080-corrupt.ihub");
        corruptPath.deleteFile();
        corruptPath.replaceWithText ("this is not a zip archive");

        juce::String loadError;
        ihub::project::SetFile untouched;
        untouched.name = "sentinel";

        check (! ihub::project::Serializer::load (corruptPath, untouched, loadError),
               "a corrupt container is rejected");
        check (untouched.name == "sentinel",
               "a corrupt load leaves the destination untouched");
        corruptPath.deleteFile();
    }

    {
        juce::ZipFile source (setPath);
        const auto manifest = readEntry (source, "set.json");
        auto changed = manifest;
        const auto versionPosition = changed.find ("\"schemaVersion\"");
        check (versionPosition != std::string::npos,
               "set.json has a schemaVersion field");

        if (versionPosition != std::string::npos)
        {
            const auto colon = changed.find (':', versionPosition);

            if (colon != std::string::npos)
            {
                const auto valueStart = changed.find_first_not_of (" \t", colon + 1);
                const auto valueEnd = changed.find_first_of (",}\n", valueStart);
                changed.replace (valueStart, valueEnd - valueStart, "999");
            }
        }

        const auto changedPath = directory.getChildFile ("t080-schema-v999.ihub");
        changedPath.deleteFile();

        juce::ZipFile::Builder builder;
        const juce::MemoryBlock manifestData (changed.data(), changed.size());
        builder.addEntry (std::make_unique<juce::MemoryInputStream> (manifestData, true),
                          6,
                          "set.json",
                          juce::Time::getCurrentTime());

        const auto mapping = readEntry (source, "mapping.yaml");
        const juce::MemoryBlock mappingData (mapping.data(), mapping.size());
        builder.addEntry (std::make_unique<juce::MemoryInputStream> (mappingData, true),
                          6,
                          "mapping.yaml",
                          juce::Time::getCurrentTime());

        for (const auto& state : original.pluginStates)
        {
            const auto name = juce::String (ihub::project::pluginStateEntryPath (state.uuid));
            const auto bytes = readEntry (source, name);
            const juce::MemoryBlock data (bytes.data(), bytes.size());
            builder.addEntry (std::make_unique<juce::MemoryInputStream> (data, true),
                              6,
                              name,
                              juce::Time::getCurrentTime());
        }

        {
            juce::FileOutputStream output (changedPath);
            check (output.openedOk(), "the modified archive can be written");
            check (builder.writeToStream (output, nullptr), "the modified archive is written");
            output.flush();
        }

        juce::String loadError;
        ihub::project::SetFile rejected;
        check (! ihub::project::Serializer::load (changedPath, rejected, loadError),
               "an unsupported schema version is rejected");
        check (loadError.contains ("schema version"),
               "the schema error names the version");
        changedPath.deleteFile();
    }

    setPath.deleteFile();
    reloadedPath.deleteFile();

    if (failures != 0)
    {
        std::cout << "FAIL: set file (" << failures << " checks)" << std::endl;
        return 1;
    }

    std::cout << "PASS: set file" << std::endl;
    return 0;
}
