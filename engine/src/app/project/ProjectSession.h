#pragma once

#include <juce_core/juce_core.h>

namespace ihub {
struct SessionConfig;
}

namespace ihub::app {

bool loadProjectSession(const juce::File& projectFile,
                        ihub::SessionConfig& config, juce::String& error);

}
