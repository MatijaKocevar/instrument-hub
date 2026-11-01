#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace PluginFxTest
{

int runGainAndLatency (juce::AudioPluginFormatManager& manager,
                       const juce::PluginDescription& description);

}
