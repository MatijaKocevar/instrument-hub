#pragma once

#include "Session.h"

#include <juce_core/juce_core.h>

#include <array>
#include <string>
#include <vector>

namespace ConcurrentInstances
{

inline constexpr int kSlots = 8;
inline constexpr int kBlockSize = 128;
inline constexpr double kSampleRate = 48000.0;

extern const std::array<double, kSlots> kFrequencies;
extern const std::array<float, kSlots> kStateMarkers;

ihub::SessionConfig makeConfig (const juce::String& bundlePath);
juce::File makeWineBundle (const juce::File& source, const juce::File& root);

bool waitForAllWarm (ihub::Session& session, int timeoutMs = 30000);
bool waitForWarm (ihub::Session& session, int slot, int timeoutMs = 30000);

float normaliseFrequency (double hz);
double denormaliseFrequency (float normalised);

void applyPreset (ihub::Session& session, int slot);
double measurePitch (ihub::Session& session, int slot);

juce::String stateHash (const juce::MemoryBlock& state);
juce::String storedStateHash (const ihub::Session& session, int slot);

} // namespace ConcurrentInstances
