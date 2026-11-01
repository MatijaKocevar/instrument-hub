#pragma once

#include <juce_core/juce_core.h>

namespace ihub::project::serializer
{

/** Reads one ZIP entry, rejecting missing, oversized, truncated and corrupt
    entries before the caller sees any bytes.
*/
bool readEntry (juce::ZipFile& zip,
                const juce::String& name,
                juce::int64 maximumBytes,
                juce::MemoryBlock& destination,
                juce::String& error);

/** Adds one in-memory entry to a ZIP under construction. */
bool addEntry (juce::ZipFile::Builder& builder,
               const juce::MemoryBlock& data,
               const juce::String& name,
               int compressionLevel);

} // namespace ihub::project::serializer
