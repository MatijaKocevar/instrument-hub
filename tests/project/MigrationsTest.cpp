// ihub-migrations-test: acceptance check for T082 (set migration).
//
// The central case is the committed v1 fixture, `fixtures/set-v1.ihub`: a real
// `.ihub` container whose `set.json` declares schemaVersion 1 and therefore
// has no `notes` field and no per-slot `colour`. Loading it through
// `Serializer::load()` must migrate it to a complete current-version document:
// every v1 value preserved, and the v2/v3 defaults filled in.
//
// The chain itself is also exercised directly: migration is idempotent, a
// newer schema is rejected instead of guessed at, and an explicit value is
// never overwritten by a default.

#include "project/Migrations.h"
#include "project/SetFile.h"
#include "project/Serializer.h"

#include <juce_events/juce_events.h>

#include <cmath>
#include <iostream>
#include <memory>
#include <string>

#ifndef IHUB_V1_SET_FIXTURE
 #define IHUB_V1_SET_FIXTURE "set-v1.ihub"
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

int readVersion (const juce::var& root)
{
    if (! root.isObject())
        return -1;

    return static_cast<int> (root.getDynamicObject()->getProperty ("schemaVersion"));
}

juce::var parseDocument (const char* text)
{
    return juce::JSON::parse (juce::String (text));
}

const char* const legacyMapping =
    "schemaVersion: 1\n"
    "# Legacy mapping stored by the v1 set format.\n";

} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    using namespace ihub::project;

    const juce::File fixture (IHUB_V1_SET_FIXTURE);
    check (fixture.existsAsFile(), std::string ("the v1 fixture exists: ")
                                      + IHUB_V1_SET_FIXTURE);

    if (! fixture.existsAsFile())
        return 1;

    // --- The fixture really is a v1 document --------------------------------
    {
        juce::ZipFile zip (fixture);
        const auto manifest = readEntry (zip, "set.json");
        const auto root = juce::JSON::parse (juce::String (manifest));

        check (readVersion (root) == 1, "the fixture declares schemaVersion 1");

        const auto* object = root.getDynamicObject();
        check (object != nullptr && ! object->hasProperty ("notes"),
               "the v1 fixture has no notes field");

        const auto slotsValue = object != nullptr ? object->getProperty ("slots") : juce::var();
        const auto* slots = slotsValue.getArray();
        check (slots != nullptr && slots->size() == 2, "the v1 fixture has two slots");
        check (slots != nullptr
                   && ! slots->getReference (0).getDynamicObject()->hasProperty ("colour"),
               "the v1 fixture has no slot colour");
    }

    // --- Load the v1 fixture through the serializer ------------------------
    SetFile loaded;
    juce::String error;

    check (Serializer::load (fixture, loaded, error),
           "a v1 container loads: " + error.toStdString());
    check (loaded.schemaVersion == currentSetSchemaVersion,
           "the loaded set is at the current schema version");
    check (currentSetSchemaVersion == 3, "the current schema version is 3");
    check (loaded.validate (error),
           "the migrated set validates: " + error.toStdString());

    // v1 values are preserved.
    check (loaded.name == "Legacy v1 Set", "the set name survives migration");
    check (loaded.notes.empty(), "v1 gets the empty notes default");
    check (nearlyEqual (static_cast<float> (loaded.transport.bpm), 98.0f),
           "transport.bpm survives migration");
    check (nearlyEqual (static_cast<float> (loaded.transport.positionPpq), 0.0f),
           "transport.positionPpq survives migration");
    check (! loaded.transport.playing, "transport.playing survives migration");

    check (loaded.slots.size() == 2, "the migrated set has two slots");

    if (loaded.slots.size() == 2)
    {
        const auto& piano = loaded.slots[0];
        check (piano.index == 0 && piano.name == "Piano", "slot 0 keeps its identity");
        check (nearlyEqual (piano.gain, 0.8f), "slot 0 keeps its gain");
        check (piano.active && ! piano.muted, "slot 0 keeps its state");
        check (piano.colour == defaultSlotColour, "slot 0 gets the v2 colour default");
        check (piano.colorIndex == 0 && nearlyEqual (piano.pan, 0.0f) && ! piano.solo,
               "slot 0 gets the v3 patch defaults");
        check (piano.instrument.kind == SourceKind::plugin
                   && piano.instrument.id == "piano"
                   && piano.instrument.pluginState == "legacy-piano-state",
               "slot 0 keeps its plugin reference");
        check (nearlyEqual (piano.instrument.gain, 0.9f),
               "slot 0 keeps its instrument gain");
        check (piano.fxChain.size() == 1, "slot 0 keeps its FX chain");
        check (piano.fxChain.size() == 1
                   && piano.fxChain[0].id == "eq"
                   && nearlyEqual (piano.fxChain[0].parameters.at ("low"), 0.5f)
                   && nearlyEqual (piano.fxChain[0].parameters.at ("high"), -0.25f),
               "slot 0 keeps its FX parameters");

        const auto& input = loaded.slots[1];
        check (input.index == 1 && input.name == "Input", "slot 1 keeps its identity");
        check (nearlyEqual (input.gain, 0.5f), "slot 1 keeps its gain");
        check (! input.active && input.muted, "slot 1 keeps its mute state");
        check (input.colour == defaultSlotColour, "slot 1 gets the v2 colour default");
        check (input.colorIndex == 0 && nearlyEqual (input.pan, 0.0f) && ! input.solo,
               "slot 1 gets the v3 patch defaults");
        check (input.instrument.kind == SourceKind::audioInput
                   && input.instrument.inputChannel == 1
                   && input.instrument.directMonitor,
               "slot 1 keeps its audio input settings");
    }

    check (loaded.pluginStates.size() == 1, "the plugin state chunk survives migration");

    if (loaded.pluginStates.size() == 1)
    {
        const auto& chunk = loaded.pluginStates[0];
        check (chunk.uuid == "legacy-piano-state", "the chunk keeps its uuid");
        check (chunk.format == "VST3" && chunk.pluginId == "com.instrumenthub.legacy",
               "the chunk keeps its plugin identity");
        check (chunk.data.getSize() == 11, "the chunk keeps its byte count");
        check (chunk.data.getSize() == 11
                   && std::string (static_cast<const char*> (chunk.data.getData()),
                                   chunk.data.getSize()) == "legacy-blob",
               "the chunk bytes survive migration");
    }

    check (loaded.mappingYaml == legacyMapping, "mapping.yaml survives migration byte for byte");

    // --- The migrated set is written back as a current document -----------
    const juce::File migratedPath =
        juce::File::getCurrentWorkingDirectory().getChildFile ("t082-migrated-v3.ihub");
    migratedPath.deleteFile();

    check (Serializer::save (loaded, migratedPath, error),
           "the migrated set saves as the current version: " + error.toStdString());

    {
        juce::ZipFile zip (migratedPath);
        const auto manifest = readEntry (zip, "set.json");
        const auto root = juce::JSON::parse (juce::String (manifest));

        check (readVersion (root) == currentSetSchemaVersion,
               "the saved manifest declares schemaVersion 3");

        const auto* object = root.getDynamicObject();
        check (object != nullptr && object->hasProperty ("notes"),
               "the saved manifest has the v2 notes field");
        check (object != nullptr && object->getProperty ("notes").isString(),
               "the saved notes field is a string");

        const auto slotsValue = object != nullptr ? object->getProperty ("slots") : juce::var();
        const auto* slots = slotsValue.getArray();
        check (slots != nullptr && slots->size() == 2, "the saved manifest keeps both slots");

        if (slots != nullptr && slots->size() == 2)
        {
            check (slots->getReference (0).getDynamicObject()->getProperty ("colour").toString()
                       == juce::String (defaultSlotColour),
                   "the saved slot 0 carries the v2 default colour");
            check (slots->getReference (1).getDynamicObject()->getProperty ("colour").toString()
                       == juce::String (defaultSlotColour),
                   "the saved slot 1 carries the v2 default colour");

            for (int i = 0; i < 2; ++i)
            {
                const auto slot = slots->getReference (i).getDynamicObject();
                check (static_cast<int> (slot->getProperty ("colorIndex")) == 0,
                       "the saved slot " + std::to_string (i)
                           + " carries the v3 colorIndex default");
                check (nearlyEqual (static_cast<float> (slot->getProperty ("pan")), 0.0f),
                       "the saved slot " + std::to_string (i)
                           + " carries the v3 pan default");
                check (! static_cast<bool> (slot->getProperty ("solo")),
                       "the saved slot " + std::to_string (i)
                           + " carries the v3 solo default");
            }
        }
    }

    {
        SetFile reloaded;
        check (Serializer::load (migratedPath, reloaded, error),
               "the migrated v2 container loads again: " + error.toStdString());
        check (reloaded == loaded, "the migrated set is deeply stable across a round-trip");
    }

    migratedPath.deleteFile();

    // --- The chain itself ---------------------------------------------------
    check (migrationStepCount (1, 2) == 1, "the chain has one step from v1 to v2");
    check (migrationStepCount (2, 3) == 1, "the chain has one step from v2 to v3");
    check (migrationStepCount (1, 3) == 2, "the chain has two steps from v1 to v3");
    check (migrationStepCount (1, 1) == 0, "no steps are needed from v1 to v1");
    check (migrationStepCount (2, 2) == 0, "no steps are needed from v2 to v2");
    check (migrationStepCount (3, 3) == 0, "no steps are needed from v3 to v3");
    check (migrationStepCount (2, 1) == -1, "the chain cannot go backwards");
    check (migrationStepCount (0, 3) == -1, "a version older than the oldest is not connected");
    check (migrationStepCount (1, 4) == -1, "the chain does not reach a future version");

    const auto* firstStep = findMigrationStep (1);
    check (firstStep != nullptr && firstStep->toVersion == 2,
           "the v1 step upgrades to v2");
    check (findMigrationStep (2) != nullptr && findMigrationStep (2)->toVersion == 3,
           "the v2 step upgrades to v3");
    check (findMigrationStep (3) == nullptr, "there is no step beyond v3");
    check (findMigrationStep (0) == nullptr, "there is no step below v1");

    // --- Migration fills missing defaults only ------------------------------
    {
        auto document = parseDocument (R"({
            "schemaVersion": 1,
            "name": "Direct v1",
            "slots": [
                { "index": 0, "colour": "#ff0000" },
                { "index": 1 }
            ]
        })");

        juce::String migrationError;
        check (migrateManifest (document, migrationError),
               "a v1 document migrates: " + migrationError.toStdString());

        const auto* object = document.getDynamicObject();
        check (object != nullptr && readVersion (document) == 3,
               "the migrated document declares v3");
        check (object != nullptr && object->hasProperty ("notes")
                   && object->getProperty ("notes").toString().isEmpty(),
               "the notes default is empty");

        const auto slotsValue = object != nullptr ? object->getProperty ("slots") : juce::var();
        const auto* slots = slotsValue.getArray();
        check (slots != nullptr && slots->size() == 2, "both slots are still present");
        check (slots != nullptr && slots->size() == 2
                   && slots->getReference (0).getDynamicObject()->getProperty ("colour").toString()
                          == "#ff0000",
               "an explicit colour is not overwritten");
        check (slots != nullptr && slots->size() == 2
                   && slots->getReference (1).getDynamicObject()->getProperty ("colour").toString()
                          == juce::String (defaultSlotColour),
               "a missing colour gets the v2 default");
        check (slots != nullptr && slots->size() == 2
                   && static_cast<int> (slots->getReference (0)
                                            .getDynamicObject()
                                            ->getProperty ("colorIndex")) == 0
                   && ! static_cast<bool> (slots->getReference (0)
                                               .getDynamicObject()
                                               ->getProperty ("solo")),
               "a missing v3 patch field gets its default");

        check (migrateManifest (document, migrationError),
               "migrating an already-current document succeeds: "
                   + migrationError.toStdString());
        check (readVersion (document) == 3, "migrating twice is idempotent");
    }

    // --- v2 -> v3 fills only the new patch fields ---------------------------
    {
        auto document = parseDocument (R"({
            "schemaVersion": 2,
            "name": "Direct v2",
            "notes": "",
            "slots": [
                { "index": 0, "colour": "#ff0000", "pan": -0.5, "colorIndex": 4,
                  "solo": true },
                { "index": 1, "colour": "#00ff00" }
            ]
        })");

        juce::String migrationError;
        check (migrateManifest (document, migrationError),
               "a v2 document migrates: " + migrationError.toStdString());

        check (readVersion (document) == 3, "the migrated v2 document declares v3");

        const auto slotsValue = document.getDynamicObject()->getProperty ("slots");
        const auto* slots = slotsValue.getArray();
        check (slots != nullptr && slots->size() == 2, "both v2 slots survive");

        if (slots != nullptr && slots->size() == 2)
        {
            const auto first = slots->getReference (0).getDynamicObject();
            const auto second = slots->getReference (1).getDynamicObject();

            check (nearlyEqual (static_cast<float> (first->getProperty ("pan")), -0.5f)
                       && static_cast<int> (first->getProperty ("colorIndex")) == 4
                       && static_cast<bool> (first->getProperty ("solo")),
                   "explicit v3 patch fields are not overwritten");
            check (nearlyEqual (static_cast<float> (second->getProperty ("pan")), 0.0f)
                       && static_cast<int> (second->getProperty ("colorIndex")) == 0
                       && ! static_cast<bool> (second->getProperty ("solo")),
                   "missing v3 patch fields get neutral defaults");
        }
    }

    // --- Rejections ---------------------------------------------------------
    {
        auto future = parseDocument (R"({ "schemaVersion": 999, "name": "Future" })");
        juce::String migrationError;
        check (! migrateManifest (future, migrationError),
               "a future schema version is rejected");
        check (migrationError.contains ("schema version"),
               "the rejection names the schema version");
        check (migrationError.contains ("999"), "the rejection names the offending version");
    }

    {
        auto ancient = parseDocument (R"({ "schemaVersion": 0, "name": "Ancient" })");
        juce::String migrationError;
        check (! migrateManifest (ancient, migrationError),
               "a version older than the oldest is rejected");
        check (migrationError.contains ("schema version"),
               "the old-version rejection names the schema version");
    }

    {
        auto missing = parseDocument (R"({ "name": "No version" })");
        juce::String migrationError;
        check (! migrateManifest (missing, migrationError),
               "a document without schemaVersion is rejected");
        check (migrationError.contains ("schemaVersion"),
               "the missing-version error names the field");
    }

    {
        auto wrongType = parseDocument (R"({ "schemaVersion": "1", "name": "String version" })");
        juce::String migrationError;
        check (! migrateManifest (wrongType, migrationError),
               "a non-integer schemaVersion is rejected");
    }

    if (failures != 0)
    {
        std::cout << "FAIL: set migration (" << failures << " checks)" << std::endl;
        return 1;
    }

    std::cout << "PASS: set migration" << std::endl;
    return 0;
}
