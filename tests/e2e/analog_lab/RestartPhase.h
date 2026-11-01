#pragma once

#include <juce_core/juce_core.h>

#include <string>

namespace AnalogLab
{

/** Second child process: fresh session from the sidecar, hashes and audio. */
int runRestartPhase (const juce::File& stateDirectory,
                     const std::string& hashA,
                     const std::string& hashB);

} // namespace AnalogLab
