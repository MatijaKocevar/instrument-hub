#pragma once

#include <juce_core/juce_core.h>

#include <cstdint>

namespace ihub::session {

struct PluginStateFileData {
    juce::MemoryBlock data;
    juce::String pluginPath;
    juce::String pluginId;
    juce::String pluginName;
    std::uint64_t revision = 0;
};

juce::File pluginStateFileFor(const juce::File& directory, int slot);

bool writePluginStateFile(const juce::File& file, int slot, const juce::MemoryBlock& data,
                          const juce::String& pluginPath, const juce::String& pluginId,
                          const juce::String& pluginName, std::uint64_t revision);

bool readPluginStateFileData(const juce::File& file, int expectedSlot, PluginStateFileData& result);

bool pluginStateMatches(const PluginStateFileData& stored, const juce::String& pluginPath,
                        const juce::String& pluginId);

}  // namespace ihub::session
