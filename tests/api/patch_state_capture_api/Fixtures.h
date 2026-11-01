#pragma once

#include "Session.h"

#include <juce_core/juce_core.h>

#include <string>

namespace ApiTest::PatchStateCaptureApi
{

constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 128;
constexpr int kSlots = 3;
constexpr int kPluginSlot = 0;
constexpr int kToneSlot = 1;

constexpr double kFrequencyA = 330.0;
constexpr double kFrequencyB = 660.0;
constexpr float kStateMarkerA = 0.3f;
constexpr float kStateMarkerB = 0.7f;

juce::File makeTempRoot();

juce::MemoryBlock readBytes (const juce::File& file);

ihub::SessionConfig makeConfig (const juce::String& bundlePath);

void applyPreset (ihub::Session& session,
                  int slot,
                  double frequency,
                  float stateMarker);

float stateMarker (ihub::Session& session, int slot);

double measurePitch (ihub::Session& session);

double denormaliseFrequency (float normalised);

bool approximately (double value, double expected, double tolerance);

}
