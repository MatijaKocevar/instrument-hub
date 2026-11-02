#include "multipatch_batch/RestorePhase.h"
#include "multipatch_batch/BatchRig.h"
#include "multipatch_batch/Constants.h"
#include "multipatch_batch/Patches.h"

#include "Check.h"
#include "Session.h"
#include "plugins/SandboxHost.h"

#include <string>

namespace MultiPatch
{
using E2E::check;

bool restoreBatch (int batchIndex,
                   const juce::File& bundle,
                   const juce::File& stateDirectory,
                   const PresetStates& states,
                   BatchRecord& record)
{
    const int before = E2E::failures;

    ihub::Session session;

    if (! buildSession (session, makeBatchConfig (bundle.getFullPathName(), batchIndex),
                        stateDirectory))
        return false;

    record.patches.resize (states.size());

    for (int local = 0; local < static_cast<int> (states.size()); ++local)
    {
        const auto preset = presetForPatch (firstPatchOfBatch (batchIndex) + local);
        auto& patchRecord = record.patches[static_cast<std::size_t> (local)];
        patchRecord.patch = preset.index;

        const auto label = "batch " + std::to_string (batchIndex) + " patch "
                         + std::to_string (preset.index);

        check (session.hasPluginState (preset.index),
               label + " loaded its sidecar state at startup");

        if (! armAndWait (session, preset.index))
        {
            check (false, label + " restored from the sidecar");
            continue;
        }

        auto* host = session.getSandboxHost (preset.index);

        if (host == nullptr)
        {
            check (false, label + " runs in the plugin sandbox after restart");
            continue;
        }

        check (session.getSandboxProcessId (preset.index) > 0,
               label + " has a sandbox helper process after restart");

        const auto live = host->getStateInformation();
        patchRecord.restoreHash = hashOf (live);

        check (live == states[static_cast<std::size_t> (local)],
               label + " restored its own preset (" + patchRecord.restoreHash + ")");
        check (patchRecord.restoreHash == patchRecord.captureHash,
               label + " restored the state it captured (" + patchRecord.captureHash
                   + " -> " + patchRecord.restoreHash + ")");
    }

    record.peakLiveInstances = juce::jmax (record.peakLiveInstances,
                                           session.getPeakInstrumentCount());

    check (session.getPeakInstrumentCount() <= 1 + kNeighbourCount,
           "batch " + std::to_string (batchIndex)
               + " kept at most two live plugin instances after restart (peak "
               + std::to_string (session.getPeakInstrumentCount()) + ")");

    return E2E::failures == before;
}

}
