#pragma once

#include <juce_core/juce_core.h>

namespace AnalogLab
{

/** First child process: capture two states, switch A -> B -> A, render audio. */
int runFirstPhase (const juce::File& stateDirectory, const juce::File& hashFile);

} // namespace AnalogLab
