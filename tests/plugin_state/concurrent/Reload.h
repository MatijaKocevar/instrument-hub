#pragma once

#include "ConcurrentFixture.h"
#include "EditorCapture.h"

#include "Session.h"

#include <juce_core/juce_core.h>

#include <vector>

namespace ConcurrentInstances
{

inline constexpr int kReloadSlot = 3;

std::vector<juce::String> runReload (ihub::Session& session,
                                     const Check& check);

void runRestored (ihub::Session& session,
                  const std::vector<juce::String>& expectedHashes,
                  const Check& check);

} // namespace ConcurrentInstances
