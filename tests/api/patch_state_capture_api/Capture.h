#pragma once

#include <juce_core/juce_core.h>

namespace ApiTest::PatchStateCaptureApi
{

bool runCapture (const juce::File& stateDirectory, const juce::String& bundlePath);

}
