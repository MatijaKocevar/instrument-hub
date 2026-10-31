#include "NoteTonePluginProcessor.h"

// Entry point used by the JUCE VST3 wrapper.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NoteTonePluginProcessor();
}
