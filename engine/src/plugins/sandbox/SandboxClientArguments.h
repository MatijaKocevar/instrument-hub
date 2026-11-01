#pragma once

#include <juce_core/juce_core.h>

#include <vector>

namespace ihub::plugins::sandbox_client
{
struct ParameterAssignment
{
    juce::String name;
    float value = 0.0f;
};

struct ChildArguments
{
    juce::File mapFile;
    juce::File pluginBundle;
    double fakeToneFrequency = 0.0;
    int abortAfterBlocks = -1;
    std::vector<ParameterAssignment> parameters;

    bool valid = false;
    juce::String error;
};

ChildArguments parseArguments (const juce::StringArray& commandLine);

} // namespace ihub::plugins::sandbox_client
