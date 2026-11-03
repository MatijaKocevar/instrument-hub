#include "Migrations.h"

#include <cmath>

namespace ihub::project
{

namespace
{

constexpr double maximumSchemaVersion = 1000000.0;

bool readSchemaVersion (const juce::var& root, int& version, juce::String& error)
{
    if (! root.isObject())
    {
        error = "set.json is not a JSON object";
        return false;
    }

    const auto* value = root.getDynamicObject()->getProperties().getVarPointer ("schemaVersion");

    if (value == nullptr)
    {
        error = "set.json is missing \"schemaVersion\"";
        return false;
    }

    if (! (value->isInt() || value->isInt64() || value->isDouble()))
    {
        error = "set.schemaVersion: expected an integer";
        return false;
    }

    const auto number = static_cast<double> (*value);

    if (! std::isfinite (number)
        || std::abs (number - std::floor (number)) > 1.0e-9
        || number < 0.0
        || number > maximumSchemaVersion)
    {
        error = "set.schemaVersion: expected an integer between 0 and "
                + juce::String (static_cast<int> (maximumSchemaVersion));
        return false;
    }

    version = static_cast<int> (number);
    return true;
}

bool migrateV1ToV2 (juce::var& root, juce::String& error)
{
    auto* set = root.getDynamicObject();

    if (set == nullptr)
    {
        error = "set.json is not a JSON object";
        return false;
    }

    if (! set->hasProperty ("notes"))
        set->setProperty ("notes", juce::String());

    auto* slots = set->getProperties().getVarPointer ("slots");

    if (slots == nullptr || ! slots->isArray())
    {
        return true;
    }

    auto* array = slots->getArray();

    for (int i = 0; i < array->size(); ++i)
    {
        auto& value = array->getReference (i);

        if (! value.isObject())
            continue;

        auto* slot = value.getDynamicObject();

        if (! slot->hasProperty ("colour"))
            slot->setProperty ("colour", juce::String (defaultSlotColour));
    }

    return true;
}

bool migrateV2ToV3 (juce::var& root, juce::String& error)
{
    auto* set = root.getDynamicObject();

    if (set == nullptr)
    {
        error = "set.json is not a JSON object";
        return false;
    }

    auto* slots = set->getProperties().getVarPointer ("slots");

    if (slots == nullptr || ! slots->isArray())
    {
        return true;
    }

    auto* array = slots->getArray();

    for (int i = 0; i < array->size(); ++i)
    {
        auto& value = array->getReference (i);

        if (! value.isObject())
            continue;

        auto* slot = value.getDynamicObject();

        if (! slot->hasProperty ("colorIndex"))
            slot->setProperty ("colorIndex", 0);

        if (! slot->hasProperty ("pan"))
            slot->setProperty ("pan", 0.0);

        if (! slot->hasProperty ("solo"))
            slot->setProperty ("solo", false);
    }

    return true;
}

bool migrateV3ToV4 (juce::var& root, juce::String& error)
{
    auto* set = root.getDynamicObject();

    if (set == nullptr)
    {
        error = "set.json is not a JSON object";
        return false;
    }

    auto* slots = set->getProperties().getVarPointer ("slots");

    if (slots == nullptr || ! slots->isArray())
    {
        return true;
    }

    auto* array = slots->getArray();

    for (int i = 0; i < array->size(); ++i)
    {
        auto& value = array->getReference (i);

        if (! value.isObject())
            continue;

        auto* slot = value.getDynamicObject();
        auto* instrument = slot->getProperties().getVarPointer ("instrument");

        if (instrument == nullptr || ! instrument->isObject())
            continue;

        auto* instrumentObject = instrument->getDynamicObject();

        if (! instrumentObject->hasProperty ("pluginPath"))
            instrumentObject->setProperty ("pluginPath", juce::String());
    }

    return true;
}

}

const std::vector<MigrationStep>& migrationSteps()
{
    static const std::vector<MigrationStep> steps
    {
        { 1, 2, &migrateV1ToV2, "add set notes and per-slot colour defaults" },
        { 2, 3, &migrateV2ToV3, "add per-patch colour, pan and solo defaults" },
        { 3, 4, &migrateV3ToV4, "add the plugin bundle path to source descriptors" }
    };

    return steps;
}

const MigrationStep* findMigrationStep (int fromVersion) noexcept
{
    for (const auto& step : migrationSteps())
        if (step.fromVersion == fromVersion)
            return &step;

    return nullptr;
}

int migrationStepCount (int fromVersion, int toVersion) noexcept
{
    if (fromVersion == toVersion)
        return 0;

    if (fromVersion < oldestSupportedSetSchemaVersion
        || toVersion > currentSetSchemaVersion)
        return -1;

    int version = fromVersion;
    int count = 0;

    while (version < toVersion)
    {
        const auto* step = findMigrationStep (version);

        if (step == nullptr || step->toVersion <= version
            || step->toVersion > currentSetSchemaVersion)
            return -1;

        version = step->toVersion;
        ++count;

        if (count > 32)
            return -1;
    }

    return version == toVersion ? count : -1;
}

bool migrateManifest (juce::var& root, juce::String& error)
{
    int version = 0;

    if (! readSchemaVersion (root, version, error))
        return false;

    if (version > currentSetSchemaVersion)
    {
        error = "unsupported set schema version " + juce::String (version)
                + " (this build supports "
                + juce::String (oldestSupportedSetSchemaVersion) + " to "
                + juce::String (currentSetSchemaVersion) + ")";
        return false;
    }

    if (version < oldestSupportedSetSchemaVersion)
    {
        error = "unsupported set schema version " + juce::String (version)
                + " (oldest supported is "
                + juce::String (oldestSupportedSetSchemaVersion) + ")";
        return false;
    }

    while (version < currentSetSchemaVersion)
    {
        const auto* step = findMigrationStep (version);

        if (step == nullptr || step->apply == nullptr || step->toVersion <= version)
        {
            error = "no migration from set schema version "
                    + juce::String (version) + " to "
                    + juce::String (currentSetSchemaVersion);
            return false;
        }

        if (! step->apply (root, error))
        {
            error = "set migration " + juce::String (step->fromVersion) + " -> "
                    + juce::String (step->toVersion) + " failed: " + error;
            return false;
        }

        root.getDynamicObject()->setProperty ("schemaVersion", step->toVersion);
        version = step->toVersion;
    }

    error.clear();
    return true;
}

}
