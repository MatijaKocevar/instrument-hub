#pragma once

#include <juce_core/juce_core.h>

#include <functional>
#include <string>

namespace ConcurrentInstances
{
using Check = std::function<void (bool, const std::string&)>;

void runSharedPage (const juce::String& bundlePath, const Check& check);
}

