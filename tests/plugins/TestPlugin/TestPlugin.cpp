#include "TestPluginProcessor.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TestPluginAudioProcessor();
}
