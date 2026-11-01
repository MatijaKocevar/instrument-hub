#pragma once

#include <juce_core/juce_core.h>

namespace ihub::app
{

juce::String getCommandLineValue (const juce::String& commandLine,
                                  const juce::String& option);

}
