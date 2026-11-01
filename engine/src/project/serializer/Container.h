#pragma once

#include <juce_core/juce_core.h>

namespace ihub::project::serializer
{

bool readEntry (juce::ZipFile& zip,
                const juce::String& name,
                juce::int64 maximumBytes,
                juce::MemoryBlock& destination,
                juce::String& error);

bool addEntry (juce::ZipFile::Builder& builder,
               const juce::MemoryBlock& data,
               const juce::String& name,
               int compressionLevel);

}
