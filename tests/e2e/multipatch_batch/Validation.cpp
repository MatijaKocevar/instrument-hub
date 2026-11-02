#include "multipatch_batch/Validation.h"
#include "multipatch_batch/Constants.h"
#include "multipatch_batch/Patches.h"

#include "Check.h"

#include <array>
#include <set>
#include <string>

namespace MultiPatch
{
using E2E::check;

ValidationSummary validateBatches (const std::vector<BatchRecord>& records)
{
    std::array<bool, kPatchCount> seen {};
    std::set<std::string> hashes;
    ValidationSummary summary;

    for (int batch = 0; batch < kBatchCount; ++batch)
    {
        const auto& record = records[static_cast<std::size_t> (batch)];
        const int first = firstPatchOfBatch (batch);

        check (record.batch == batch,
               "batch " + std::to_string (batch) + " record names the right batch");
        check (record.peakLiveInstances <= 1 + kNeighbourCount,
               "batch " + std::to_string (batch)
                   + " kept at most two live plugin instances (peak "
                   + std::to_string (record.peakLiveInstances) + ")");
        check (record.patches.size() == static_cast<std::size_t> (kBatchSize),
               "batch " + std::to_string (batch) + " covered eight patches");

        for (const auto& patch : record.patches)
        {
            const bool inRange = patch.patch >= first
                              && patch.patch < first + kBatchSize;

            check (inRange, "batch " + std::to_string (batch) + " covered patch "
                                + std::to_string (patch.patch));

            if (! inRange || patch.patch < 0 || patch.patch >= kPatchCount)
                continue;

            const auto index = static_cast<std::size_t> (patch.patch);

            check (! seen[index], "patch " + std::to_string (patch.patch)
                                      + " appeared exactly once");
            seen[index] = true;

            const bool matches = patch.captureHash.size() == 16
                              && patch.restoreHash == patch.captureHash;

            check (matches, "patch " + std::to_string (patch.patch)
                                + " restored its own preset (" + patch.restoreHash + ")");

            if (matches)
            {
                ++summary.restored;
                hashes.insert (patch.captureHash);
            }
        }
    }

    for (const bool present : seen)
        summary.covered += present ? 1 : 0;

    summary.uniqueStates = static_cast<int> (hashes.size());
    return summary;
}

}
