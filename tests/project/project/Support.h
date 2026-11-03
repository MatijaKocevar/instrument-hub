#pragma once

#include <juce_core/juce_core.h>

#include <string>

namespace ProjectTest
{

extern int failures;

void check (bool condition, const std::string& what);

juce::File makeTemporaryDirectory();

juce::File bundledMappingFile();

juce::File bundledTracksSetFile();

juce::File bundledBlankSetFile();

juce::File bundledPluginStateDirectory();

juce::File bundledSourcesFile();

}
