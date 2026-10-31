#include "TestPluginProcessor.h"

// Entry point used by the JUCE VST3 wrapper (and by the other plugin formats
// if this target ever grows beyond VST3).
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TestPluginAudioProcessor();
}
