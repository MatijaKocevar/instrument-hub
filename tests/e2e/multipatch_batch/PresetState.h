#pragma once

#include <juce_core/juce_core.h>

#include <string>
#include <vector>

namespace MultiPatch
{

using PresetStates = std::vector<juce::MemoryBlock>;

bool createPresetStates (const juce::File& bundle,
                         int firstPatch,
                         int count,
                         PresetStates& states,
                         juce::String& error);

std::string hashOf (const juce::MemoryBlock& data);

}
