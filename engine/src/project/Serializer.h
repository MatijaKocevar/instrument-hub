#pragma once

#include "SetFile.h"

#include <juce_core/juce_core.h>

namespace ihub::project
{

/**
    Reads and writes the `.ihub` set container.

    The container is an ordinary ZIP with a fixed layout:

        set.json                       the SetFile document (no inline blobs)
        mapping.yaml                   the controller mapping, byte for byte
        plugin-state/<uuid>.bin        one opaque plugin state blob per entry

    `save()` validates the document first, writes the archive to a sibling
    temporary file and only then replaces the destination, so a failed write
    cannot destroy an existing set. `load()` builds the result in a local
    object and only assigns it once the whole archive has been validated, so a
    failed load leaves the caller's SetFile untouched.

    The serializer is plain `juce_core`: no engine, plugin or UI dependency.
    It is safe to call from any non-audio thread.
*/
class Serializer final
{
public:
    Serializer() = delete;

    static constexpr const char* manifestEntry = "set.json";
    static constexpr const char* mappingEntry = "mapping.yaml";

    /** Reject anything larger than these before allocating it. */
    static constexpr juce::int64 maximumManifestBytes = 8 * 1024 * 1024;
    static constexpr juce::int64 maximumMappingBytes = 4 * 1024 * 1024;
    static constexpr juce::int64 maximumPluginStateBytes = 64 * 1024 * 1024;
    static constexpr juce::int64 maximumTotalUncompressedBytes = 256 * 1024 * 1024;

    /** Writes `set` to `file`.

        Returns false and fills `error` when the set is invalid, the archive
        cannot be written, or the destination cannot be replaced.
    */
    static bool save (const SetFile& set, const juce::File& file, juce::String& error);

    /** Reads `file` into `destination`.

        Older `set.json` documents are migrated to the current schema version
        before they are validated (see `Migrations.h`). Returns false and fills
        `error` on a missing file, a malformed or hostile archive, a document
        newer than this build, or a document that fails `SetFile::validate()`.
    */
    static bool load (const juce::File& file, SetFile& destination, juce::String& error);
};

} // namespace ihub::project
