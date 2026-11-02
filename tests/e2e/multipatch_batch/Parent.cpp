#include "multipatch_batch/Parent.h"
#include "multipatch_batch/ChildRunner.h"
#include "multipatch_batch/Constants.h"
#include "multipatch_batch/Record.h"
#include "multipatch_batch/Validation.h"

#include "Check.h"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace MultiPatch
{
using E2E::check;

namespace
{
juce::File makeTempRoot()
{
    const juce::File approvedBase ("/tmp/opencode");
    const auto base = approvedBase.isDirectory()
                          ? approvedBase
                          : juce::File::getSpecialLocation (
                                juce::File::tempDirectory);

    return base.getChildFile ("ihub-t186-multipatch-"
                              + juce::String::toHexString (
                                    juce::Random::getSystemRandom().nextInt64()));
}
}

int runParent()
{
    const juce::File bundle { juce::String (IHUB_TESTPLUGIN_VST3) };

    if (! bundle.exists())
    {
        std::cerr << "multipatch-batch-test: TestPlugin.vst3 is missing ("
                  << bundle.getFullPathName() << ")" << std::endl;
        return 1;
    }

    const auto tempRoot = makeTempRoot();
    (void) tempRoot.deleteRecursively();

    if (! tempRoot.createDirectory().wasOk())
    {
        std::cerr << "multipatch-batch-test: could not create "
                  << tempRoot.getFullPathName() << std::endl;
        return 1;
    }

    const auto executable = juce::File::getSpecialLocation (
        juce::File::currentExecutableFile);
    const auto stateDirectory = tempRoot.getChildFile ("set.plugin-state");

    std::vector<BatchRecord> records (static_cast<std::size_t> (kBatchCount));

    for (int batch = 0; batch < kBatchCount; ++batch)
    {
        const auto batchDirectory = tempRoot.getChildFile ("batch-" + juce::String (batch));
        const auto resultFile = batchDirectory.getChildFile ("result.txt");

        if (! batchDirectory.createDirectory().wasOk())
        {
            check (false, "batch " + std::to_string (batch)
                              + " directory could be created");
            continue;
        }

        const juce::StringArray arguments {
            executable.getFullPathName(),
            "--batch",
            juce::String (batch),
            stateDirectory.getFullPathName(),
            resultFile.getFullPathName()
        };

        const auto child = runChild (arguments, kChildTimeoutMs);
        printChildSummary (child.output);

        if (child.timedOut)
            check (false, "batch " + std::to_string (batch)
                              + " child finished within " + std::to_string (kChildTimeoutMs)
                              + " ms");
        else
            check (child.exitCode == 0,
                   "batch " + std::to_string (batch) + " child exited cleanly (rc "
                       + std::to_string (child.exitCode) + ")");

        check (readBatchRecord (resultFile, records[static_cast<std::size_t> (batch)]),
               "batch " + std::to_string (batch) + " wrote a readable result record");
    }

    const auto summary = validateBatches (records);

    juce::Array<juce::File> stateFiles;
    stateDirectory.findChildFiles (stateFiles, juce::File::findFiles, false,
                                   "slot-*.bin");

    check (stateFiles.size() == kPatchCount,
           "the shared sidecar holds 64 patch states (got "
               + std::to_string (stateFiles.size()) + ")");
    check (summary.covered == kPatchCount,
           "all 64 patches were covered across the batches (got "
               + std::to_string (summary.covered) + ")");
    check (summary.restored == kPatchCount,
           "all 64 patches restored their own preset (got "
               + std::to_string (summary.restored) + ")");
    check (summary.uniqueStates == kPatchCount,
           "all 64 restored presets are distinct (got "
               + std::to_string (summary.uniqueStates) + ")");

    {
        std::ofstream record ("ihub-multipatch-batch-record.txt");
        record << "test=ihub-multipatch-batch\n"
               << "patches=" << kPatchCount << "\n"
               << "batches=" << kBatchCount << "\n"
               << "batch_size=" << kBatchSize << "\n"
               << "restored=" << summary.restored << "\n"
               << "unique_states=" << summary.uniqueStates << "\n"
               << "status=" << (E2E::failures == 0 ? "PASS" : "FAIL") << "\n";

        check (record.good(), "the multipatch record was written "
                              "(ihub-multipatch-batch-record.txt)");
    }

    (void) tempRoot.deleteRecursively();

    std::cout << (E2E::failures == 0 ? "\nPASS" : "\nFAIL") << ": "
              << E2E::failures << " failed check(s)" << std::endl;

    return E2E::failures == 0 ? 0 : 1;
}

}
