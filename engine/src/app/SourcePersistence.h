#pragma once

#include <juce_core/juce_core.h>

#include <filesystem>

namespace ihub
{
struct SessionConfig;
struct SessionSlotConfig;
} // namespace ihub

namespace ihub::app
{

/** Sidecar holding the Instrument page's source assignment next to set.yaml. */
juce::File sourceStateFileFor (const std::filesystem::path& setFile);

/** Sidecar directory holding one plugin-state blob per patch (T130). */
juce::File pluginStateDirectoryFor (const std::filesystem::path& setFile);

/** Applies a descriptor's kind to the slot, resolving paths on disk. */
void applySourceKind (const juce::String& kind, ihub::SessionSlotConfig& slot);

/** Re-applies persisted source assignments before the session is built. */
void loadPersistedSources (ihub::SessionConfig& config,
                           const std::filesystem::path& setFile);

/** Persists one source assignment. Called from the session's assigned hook. */
void persistSourceAssignment (const std::filesystem::path& setFile,
                              int slot,
                              const ihub::SessionSlotConfig& source);

} // namespace ihub::app
