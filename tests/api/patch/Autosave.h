#pragma once

#include <juce_core/juce_core.h>

namespace ApiTest::PatchAutosave
{

bool runAutosave (const juce::File& root, const juce::String& bundlePath);

}
