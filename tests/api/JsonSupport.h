#pragma once

#include <juce_core/juce_core.h>

#include <string>

namespace ApiTest
{

juce::var parseJson (const std::string& text);

juce::var property (const juce::var& object, const char* name);

juce::var property (const juce::var& object, const juce::String& name);

const juce::Array<juce::var>* arrayProperty (const juce::var& object, const char* name);

double numberProperty (const juce::var& object, const char* name);

int intProperty (const juce::var& object, const char* name);

bool boolProperty (const juce::var& object, const char* name);

std::string stringProperty (const juce::var& object, const char* name);

juce::var patchForSlot (const juce::var& state, int slot);

}
