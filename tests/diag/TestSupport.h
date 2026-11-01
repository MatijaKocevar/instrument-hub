#pragma once

#include <juce_core/juce_core.h>

#include <string>

namespace DiagTest
{

extern int failures;

void check (bool condition, const std::string& what);
bool contains (const std::string& text, const std::string& needle);
juce::File tempFile (const juce::String& suffix);

}
