#pragma once

#include <juce_core/juce_core.h>

namespace ApiTest::ProjectLifecycle
{

bool runLifecycle (const juce::File& root);
bool runHistory (const juce::File& root);

}
