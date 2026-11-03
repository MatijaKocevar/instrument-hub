#pragma once

#include "settings/SettingsStore.h"

#include <juce_core/juce_core.h>

#include <string>

namespace SettingsTest
{

extern int failures;

void check (bool condition, const std::string& what);

bool nearlyEqual (double first, double second);

juce::File makeTemporaryDirectory();

juce::File settingsFile (const juce::File& directory);

void writeTextFile (const juce::File& file, const juce::String& text);

juce::String readTextFile (const juce::File& file);

juce::var readJsonFile (const juce::File& file);

juce::var property (const juce::var& object, const juce::String& name);

bool isOwnerOnly (const juce::File& file);

void runDefaultChecks();
void runRoundTripChecks();
void runCorruptRecoveryChecks();
void runUnknownKeyChecks();
void runAtomicReplaceChecks();
void runNewerVersionChecks();

}
