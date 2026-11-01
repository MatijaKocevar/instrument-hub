#pragma once

#include "Session.h"

#include <juce_core/juce_core.h>

#include <string>

namespace AnalogLab
{

std::string hashOf (const juce::MemoryBlock& data);

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

}
