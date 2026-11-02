#pragma once

#include <juce_core/juce_core.h>

namespace MultiPatch
{

int runBatchPhase (int batchIndex,
                   const juce::File& stateDirectory,
                   const juce::File& resultFile);

}
