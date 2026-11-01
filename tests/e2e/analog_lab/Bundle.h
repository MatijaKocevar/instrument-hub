#pragma once

#include <juce_core/juce_core.h>

namespace AnalogLab
{

/** An Analog Lab V bundle, if this machine has one (native, vstbridge or
    yabridge). IHUB_ANALOG_LAB_VST3 overrides the search. */
juce::File findAnalogLabBundle();

} // namespace AnalogLab
