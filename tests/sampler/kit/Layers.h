#pragma once

#include "sampler/KitLoader.h"

#include <juce_core/juce_core.h>

namespace KitTest
{

void runLayersChecks (const juce::File& directory, ihub::sampler::LoadedKit& kit);

} // namespace KitTest
