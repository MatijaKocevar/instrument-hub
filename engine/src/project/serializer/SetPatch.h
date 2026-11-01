#pragma once

#include "project/SetFile.h"

#include <juce_core/juce_core.h>

namespace ihub::project::serializer
{

/** Converts one slot and its patch (instrument plus FX chain) to JSON. */
juce::var slotToVar (const SlotConfig& slot);

/** Parses one slot and its patch (instrument plus FX chain) from JSON. */
bool parseSlot (const juce::var& value,
                const juce::String& context,
                SlotConfig& slot,
                juce::String& error);

} // namespace ihub::project::serializer
