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

struct PageSlotArguments
{
    int slotIndex = -1;
    juce::File pluginBundle;
    double fakeToneFrequency = 0.0;
    int abortAfterBlocks = -1;
    std::vector<ParameterAssignment> parameters;
};

struct ChildArguments
{
    juce::File mapFile;
    juce::File pluginBundle;
    double fakeToneFrequency = 0.0;
    int abortAfterBlocks = -1;
    std::vector<ParameterAssignment> parameters;

    juce::File pageMapFile;
    juce::String pageId;
    std::vector<PageSlotArguments> pageSlots;

    bool pageMode = false;
    bool valid = false;
    juce::String error;
};

ChildArguments parseArguments (const juce::StringArray& commandLine);

}
