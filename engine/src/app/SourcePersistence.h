#pragma once

#include <juce_core/juce_core.h>

#include <filesystem>

namespace ihub {
struct SessionConfig;
struct SessionSlotConfig;
}  // namespace ihub

namespace ihub::app {

juce::File sourceStateFileFor(const std::filesystem::path& setFile);

juce::File pluginStateDirectoryFor(const std::filesystem::path& setFile);

void applySourceKind(const juce::String& kind, ihub::SessionSlotConfig& slot);

void loadPersistedSources(ihub::SessionConfig& config, const std::filesystem::path& setFile);

void persistSourceAssignment(const std::filesystem::path& setFile, int slot,
                             const ihub::SessionSlotConfig& source);

}  // namespace ihub::app
