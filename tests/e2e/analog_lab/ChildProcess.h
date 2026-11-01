#pragma once

#include <juce_core/juce_core.h>

namespace AnalogLab
{

int runChild (const juce::File& executable,
              const juce::StringArray& arguments,
              const juce::File& logFile,
              bool printSummary);

int runChildWithRetry (const juce::File& executable,
                       const juce::StringArray& arguments,
                       const juce::File& logFile,
                       const juce::String& label);

} // namespace AnalogLab
