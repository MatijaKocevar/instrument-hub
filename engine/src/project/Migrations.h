#pragma once

#include <juce_core/juce_core.h>

#include <vector>

namespace ihub::project
{

    inline constexpr int oldestSupportedSetSchemaVersion = 1;

    inline constexpr int currentSetSchemaVersion = 4;

    inline constexpr const char* defaultSlotColour = "#5a6478";

    using MigrationFunction = bool (*) (juce::var& root, juce::String& error);

    struct MigrationStep
    {
        int fromVersion = 0;
        int toVersion = 0;
        MigrationFunction apply = nullptr;

        const char* description = nullptr;
    };

    const std::vector<MigrationStep>& migrationSteps ();

    const MigrationStep* findMigrationStep (int fromVersion) noexcept;

    int migrationStepCount (int fromVersion, int toVersion) noexcept;

    bool migrateManifest (juce::var& root, juce::String& error);

}
