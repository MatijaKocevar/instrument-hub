#pragma once

#include <juce_core/juce_core.h>

namespace MultiPatch
{

struct ChildResult
{
    int exitCode = -1;
    bool timedOut = false;
    juce::String output;
};

ChildResult runChild (const juce::StringArray& arguments, int timeoutMs);
void printChildSummary (const juce::String& output);

}
