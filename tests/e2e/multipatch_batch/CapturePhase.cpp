#include "multipatch_batch/CapturePhase.h"
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

bool captureBatch (int batchIndex,
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

        if (! armAndWait (session, preset.index))
        {
            check (false, label + " became active");
            continue;
        }

        auto* host = session.getSandboxSlotControl (preset.index);

        if (host == nullptr)
        {
            check (false, label + " runs in the plugin sandbox");
            continue;
        }

        check (session.getSandboxProcessId (preset.index) > 0,
               label + " has a sandbox helper process");

        check (host->setStateInformation (states[static_cast<std::size_t> (local)]),
               label + " accepts its preset");

        const auto live = host->getStateInformation();
        patchRecord.captureHash = hashOf (live);

        check (live == states[static_cast<std::size_t> (local)],
               label + " captured its own preset (" + patchRecord.captureHash + ")");
        check (session.capturePluginState (preset.index),
               label + " was written to the sidecar");
    }

    record.peakLiveInstances = juce::jmax (record.peakLiveInstances,
                                           session.getPeakInstrumentCount());

    check (session.getPeakInstrumentCount() <= 1 + kNeighbourCount,
           "batch " + std::to_string (batchIndex)
               + " kept at most two live plugin instances (peak "
               + std::to_string (session.getPeakInstrumentCount()) + ")");
    check (session.savePluginStates(),
           "batch " + std::to_string (batchIndex) + " saved its sidecar");

    return E2E::failures == before;
}

}
