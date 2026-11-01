#pragma once

#include <juce_core/juce_core.h>

#include "app/Modes.h"

namespace AppModesTest
{

extern int failures;

void check (bool condition, const juce::String& what);
void checkValue (const juce::String& commandLine,
                 const juce::String& option,
                 const juce::String& expected);
void checkMode (const juce::String& commandLine, ihub::app::AppMode expected);

}
