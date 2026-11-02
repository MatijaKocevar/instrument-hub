#include "multipatch_batch/Patches.h"
#include "multipatch_batch/Constants.h"

namespace MultiPatch
{
namespace
{
constexpr const char* familyIds[] = {
    "keys", "drums", "bass", "pads", "lead", "fx", "perc", "vox"
};

constexpr const char* familyNames[] = {
    "Keys", "Drums", "Bass", "Pads", "Lead", "FX", "Percussion", "Vox"
};

constexpr int familyCount = static_cast<int> (sizeof (familyIds) / sizeof (familyIds[0]));
}

const char* familyId (int familyIndex)
{
    return familyIds[juce::jlimit (0, familyCount - 1, familyIndex)];
}

const char* familyName (int familyIndex)
{
    return familyNames[juce::jlimit (0, familyCount - 1, familyIndex)];
}

int firstPatchOfBatch (int batchIndex)
{
    return juce::jlimit (0, kBatchCount - 1, batchIndex) * kBatchSize;
}

PatchPreset presetForPatch (int patchIndex)
{
    PatchPreset preset;
    preset.index = juce::jlimit (0, kPatchCount - 1, patchIndex);
    preset.familyIndex = preset.index / kBatchSize;
    preset.padInFamily = preset.index % kBatchSize;
    preset.family = familyId (preset.familyIndex);
    preset.familyName = familyName (preset.familyIndex);
    preset.name = juce::String (preset.familyName) + " Pad "
                + juce::String (preset.padInFamily + 1);
    preset.frequency = 120.0 + 290.0 * static_cast<double> (preset.index);
    preset.marker = static_cast<float> (preset.index + 1)
                  / static_cast<float> (kPatchCount + 1);
    return preset;
}

}
