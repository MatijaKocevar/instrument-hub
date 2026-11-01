#pragma once

#include "Session.h"

#include <juce_core/juce_core.h>

#include <string>

namespace AnalogLab
{

/** Stable identity hash for an opaque plugin blob (FNV-1a 64). */
std::string hashOf (const juce::MemoryBlock& data);

/** Waits until two consecutive reads agree after reloading the candidate into
    the live instance. Only one Analog Lab instance is alive at this point. */
bool stabiliseState (ihub::plugins::PluginInstrument& instrument,
                     juce::MemoryBlock& result);

bool verifySlotState (ihub::Session& session,
                      int slot,
                      const std::string& expectedHash,
                      const std::string& phase);

bool writeHashes (const juce::File& file,
                  const std::string& hashA,
                  const std::string& hashB);

bool readHashes (const juce::File& file, std::string& hashA, std::string& hashB);

} // namespace AnalogLab
