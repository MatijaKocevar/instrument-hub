#pragma once

#include "project/SetFile.h"

#include <juce_core/juce_core.h>

namespace ihub::project::serializer
{

    juce::var pluginStateToVar (const PluginStateChunk& chunk);

    bool parsePluginStates (const juce::MemoryBlock& manifest,
                            juce::ZipFile& zip,
                            SetFile& set,
                            juce::String& error);

}
