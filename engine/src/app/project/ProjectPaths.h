#pragma once

#include <juce_core/juce_core.h>

namespace ihub::app
{

juce::File defaultProjectFile();

juce::File resolveProjectFile (const juce::String& commandLine);

}
