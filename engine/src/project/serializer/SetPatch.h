#pragma once

#include "project/SetFile.h"

#include <juce_core/juce_core.h>

namespace ihub::project::serializer {

juce::var slotToVar(const SlotConfig& slot);

bool parseSlot(const juce::var& value, const juce::String& context, SlotConfig& slot,
               juce::String& error);

}  // namespace ihub::project::serializer
