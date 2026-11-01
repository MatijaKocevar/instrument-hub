#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace PluginFxTest
{

void runStateRoundTrip (juce::AudioPluginFormatManager& manager,
                        const juce::PluginDescription& description);

} // namespace PluginFxTest
