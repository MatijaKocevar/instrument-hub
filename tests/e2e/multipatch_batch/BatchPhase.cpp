#include "multipatch_batch/BatchPhase.h"
#include "multipatch_batch/CapturePhase.h"
#include "multipatch_batch/Constants.h"
#include "multipatch_batch/Patches.h"
#include "multipatch_batch/PresetState.h"
#include "multipatch_batch/Record.h"
#include "multipatch_batch/RestorePhase.h"

#include "Check.h"

#include <iostream>
#include <string>

namespace MultiPatch
{
using E2E::check;

int runBatchPhase (int batchIndex,
                   const juce::File& stateDirectory,
                   const juce::File& resultFile)
{
    if (batchIndex < 0 || batchIndex >= kBatchCount)
    {
        std::cerr << "multipatch-batch-test: batch " << batchIndex
                  << " is outside 0.." << kBatchCount - 1 << std::endl;
        return 1;
    }

    const juce::File bundle { juce::String (IHUB_TESTPLUGIN_VST3) };

    if (! bundle.exists())
    {
        std::cerr << "multipatch-batch-test: TestPlugin.vst3 is missing ("
                  << bundle.getFullPathName() << ")" << std::endl;
        return 1;
    }

    if (! stateDirectory.getParentDirectory().createDirectory().wasOk())
    {
        std::cerr << "multipatch-batch-test: could not create "
                  << stateDirectory.getParentDirectory().getFullPathName() << std::endl;
        return 1;
    }

    juce::String error;
    PresetStates states;

    if (! createPresetStates (bundle, firstPatchOfBatch (batchIndex), kBatchSize,
                              states, error))
    {
        std::cerr << "FAIL multipatch-batch-test: " << error << std::endl;
        return 1;
    }

    BatchRecord record;
    record.batch = batchIndex;
    record.patches.resize (states.size());

    for (int local = 0; local < kBatchSize; ++local)
        record.patches[static_cast<std::size_t> (local)].patch =
            firstPatchOfBatch (batchIndex) + local;

    bool ok = captureBatch (batchIndex, bundle, stateDirectory, states, record);
    ok = restoreBatch (batchIndex, bundle, stateDirectory, states, record) && ok;

    check (ok && E2E::failures == 0,
           "batch " + std::to_string (batchIndex) + " completed without failures");
    check (writeBatchRecord (resultFile, record),
           "batch " + std::to_string (batchIndex) + " wrote its result record");

    std::cout << (E2E::failures == 0 ? "PASS" : "FAIL") << ": batch "
              << batchIndex << std::endl;

    return E2E::failures == 0 ? 0 : 1;
}

}
