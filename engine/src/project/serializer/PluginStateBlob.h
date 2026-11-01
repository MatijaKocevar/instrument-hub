#pragma once

#include "project/SetFile.h"

#include <juce_core/juce_core.h>

namespace ihub::project::serializer
{

/** Converts a plugin-state descriptor (no blob bytes) to JSON. */
juce::var pluginStateToVar (const PluginStateChunk& chunk);

/** Reads every `pluginStates` entry from `set.json` and its blob from the ZIP.

    The manifest is re-parsed here because the caller has already validated it
    as an object; entry sizes are bounded by
    `Serializer::maximumPluginStateBytes`.
*/
bool parsePluginStates (const juce::MemoryBlock& manifest,
                        juce::ZipFile& zip,
                        SetFile& set,
                        juce::String& error);

} // namespace ihub::project::serializer
