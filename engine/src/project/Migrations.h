#pragma once

#include <juce_core/juce_core.h>

#include <vector>

namespace ihub::project
{

/** Oldest `set.json` schema version this build can read.

    Older documents are upgraded in memory by `migrateManifest()`; they are
    never written back in their old form. A document older than this is
    rejected with an error instead of being guessed at.
*/
inline constexpr int oldestSupportedSetSchemaVersion = 1;

/** The `set.json` schema version this build writes.

    Version history:

    - 1: the original `.ihub` set file format (T080).
    - 2: adds `notes` on the set and `colour` on every slot. Both are filled
      in by the 1 -> 2 migration, so a v1 document loads unchanged.
*/
inline constexpr int currentSetSchemaVersion = 2;

/** Slot colour assigned to every v1 slot by the 1 -> 2 migration. */
inline constexpr const char* defaultSlotColour = "#5a6478";

/** Upgrades one schema version in place. Returns false and fills `error`. */
using MigrationFunction = bool (*) (juce::var& root, juce::String& error);

/** One link in the migration chain. */
struct MigrationStep
{
    int fromVersion = 0;
    int toVersion = 0;
    MigrationFunction apply = nullptr;

    /** Short description of what the step adds. */
    const char* description = nullptr;
};

/** The registered chain, oldest first. */
const std::vector<MigrationStep>& migrationSteps();

/** The step that upgrades `fromVersion`, or nullptr when the chain ends there. */
const MigrationStep* findMigrationStep (int fromVersion) noexcept;

/** Number of steps needed to move from `fromVersion` to `toVersion`.

    Returns 0 for an already-current document and -1 when the two versions are
    not connected by the chain (or the target is newer than this build).
*/
int migrationStepCount (int fromVersion, int toVersion) noexcept;

/** Upgrades a parsed `set.json` object to `currentSetSchemaVersion` in place.

    `root` must be a JSON object with an integer `schemaVersion` field. Each
    applicable step rewrites only the fields that are missing at that version,
    so values that are already present are preserved, and advances the
    `schemaVersion` field as it goes.

    A document newer than this build is rejected rather than guessed at; so is
    a version older than `oldestSupportedSetSchemaVersion`. On failure `error`
    describes the problem and `root` may be partly migrated (callers discard
    it), which is why the serializer only ever migrates a local copy.
*/
bool migrateManifest (juce::var& root, juce::String& error);

} // namespace ihub::project
