#pragma once

#include <juce_core/juce_core.h>

namespace ihub::app
{

/** Returns the value following `option`, or an empty string.

    Supports both `--option value` and `--option=value`.
*/
juce::String getCommandLineValue (const juce::String& commandLine,
                                  const juce::String& option);

} // namespace ihub::app
