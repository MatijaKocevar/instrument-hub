#pragma once

#include <juce_core/juce_core.h>

namespace MultiPatch
{

struct PatchPreset
{
    int index = 0;
    int familyIndex = 0;
    int padInFamily = 0;
    juce::String family;
    juce::String familyName;
    juce::String name;
    double frequency = 440.0;
    float marker = 0.5f;
};

const char* familyId (int familyIndex);
const char* familyName (int familyIndex);
int firstPatchOfBatch (int batchIndex);
PatchPreset presetForPatch (int patchIndex);

}
