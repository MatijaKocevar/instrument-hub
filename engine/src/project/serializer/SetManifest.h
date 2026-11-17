#pragma once

#include "project/SetFile.h"

#include <juce_core/juce_core.h>

namespace ihub::project::serializer {

juce::String makeManifest(const SetFile& set);

bool parseManifest(const juce::MemoryBlock& manifest, SetFile& set,
                   juce::int64& declaredMappingBytes, juce::String& error);

}  // namespace ihub::project::serializer
