#pragma once

#include <juce_core/juce_core.h>

#include <string>

namespace AnalogLab
{

int runRestartPhase (const juce::File& stateDirectory,
                     const std::string& hashA,
                     const std::string& hashB);

}
