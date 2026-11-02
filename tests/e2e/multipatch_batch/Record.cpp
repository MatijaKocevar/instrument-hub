#include "multipatch_batch/Record.h"

#include <utility>

namespace MultiPatch
{

bool writeBatchRecord (const juce::File& file, const BatchRecord& record)
{
    juce::String text;
    text << "batch," << record.batch << "\n";
    text << "peak," << record.peakLiveInstances << "\n";

    for (const auto& patch : record.patches)
        text << "patch," << patch.patch << ","
             << juce::String (patch.captureHash) << ","
             << juce::String (patch.restoreHash) << "\n";

    return file.replaceWithText (text);
}

bool readBatchRecord (const juce::File& file, BatchRecord& record)
{
    if (! file.existsAsFile())
        return false;

    record = {};

    juce::StringArray lines;
    lines.addLines (file.loadFileAsString());

    for (const auto& line : lines)
    {
        const auto fields = juce::StringArray::fromTokens (line, ",", "");

        if (fields.size() < 2)
            continue;

        if (fields[0] == "batch")
        {
            record.batch = fields[1].getIntValue();
        }
        else if (fields[0] == "peak")
        {
            record.peakLiveInstances = fields[1].getIntValue();
        }
        else if (fields[0] == "patch" && fields.size() >= 4)
        {
            PatchRecord patch;
            patch.patch = fields[1].getIntValue();
            patch.captureHash = fields[2].toStdString();
            patch.restoreHash = fields[3].toStdString();
            record.patches.push_back (std::move (patch));
        }
    }

    return record.batch >= 0;
}

}
