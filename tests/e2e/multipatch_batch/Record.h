#pragma once

#include <juce_core/juce_core.h>

#include <string>
#include <vector>

namespace MultiPatch
{

struct PatchRecord
{
    int patch = -1;
    std::string captureHash;
    std::string restoreHash;
};

struct BatchRecord
{
    int batch = -1;
    int peakLiveInstances = 0;
    std::vector<PatchRecord> patches;
};

bool writeBatchRecord (const juce::File& file, const BatchRecord& record);
bool readBatchRecord (const juce::File& file, BatchRecord& record);

}
