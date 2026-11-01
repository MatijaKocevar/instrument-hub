#pragma once

#include <juce_core/juce_core.h>

#include <string>

namespace ApiTest
{

int insertCount (const juce::var& chain);

std::string insertType (const juce::var& chain, int index);

int chainLatency (const juce::var& chain);

}
