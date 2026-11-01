#pragma once

#include "project/SetFile.h"

#include <juce_core/juce_core.h>

namespace ihub::project::serializer
{

/** Builds `set.json`: set metadata, transport, slots and plugin-state
    descriptors. Blob bytes stay in their own container entries.
*/
juce::String makeManifest (const SetFile& set);

/** Parses `set.json` into `set`.

    Migrates older documents to the current schema first, then reads the set
    metadata, transport, declared mapping size and every slot. `pluginStates`
    is parsed separately because it needs the ZIP entry reader.
*/
bool parseManifest (const juce::MemoryBlock& manifest,
                    SetFile& set,
                    juce::int64& declaredMappingBytes,
                    juce::String& error);

} // namespace ihub::project::serializer
