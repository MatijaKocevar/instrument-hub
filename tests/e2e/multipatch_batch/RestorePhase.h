#pragma once

#include "multipatch_batch/PresetState.h"
#include "multipatch_batch/Record.h"

#include <juce_core/juce_core.h>

namespace MultiPatch
{

bool restoreBatch (int batchIndex,
                   const juce::File& bundle,
                   const juce::File& stateDirectory,
                   const PresetStates& states,
                   BatchRecord& record);

}
