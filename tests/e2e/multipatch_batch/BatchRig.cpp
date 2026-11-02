#include "multipatch_batch/BatchRig.h"
#include "multipatch_batch/Constants.h"
#include "multipatch_batch/Patches.h"

#include <juce_events/juce_events.h>

#include <iostream>
#include <string>

namespace MultiPatch
{

void pumpMessages()
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
}

bool armAndWait (ihub::Session& session, int slot)
{
    session.armSlot (slot);

    const double deadline = juce::Time::getMillisecondCounterHiRes()
                          + static_cast<double> (kArmTimeoutMs);

    if (session.getWarmState (slot) == ihub::SessionWarmState::active)
        return true;

    while (juce::Time::getMillisecondCounterHiRes() < deadline)
    {
        pumpMessages();

        if (session.getWarmState (slot) == ihub::SessionWarmState::active)
            return true;
    }

    std::cerr << "FAIL batch rig: slot " << slot << " did not become active within "
              << kArmTimeoutMs << " ms (state: " << session.getWarmStateName (slot)
              << ")" << std::endl;
    return false;
}

ihub::SessionConfig makeBatchConfig (const juce::String& bundlePath, int batchIndex)
{
    ihub::SessionConfig config;
    config.device.sampleRate = kSampleRate;
    config.device.bufferSize = kBlockSize;
    config.device.numOutputChannels = 2;
    config.device.numInputChannels = 2;
    config.fadeMilliseconds = 3.0f;
    config.neighbourCount = kNeighbourCount;
    config.initialSlot = firstPatchOfBatch (batchIndex);

    config.slots = ihub::Session::makeDefaultSet (ihub::SessionSlotKind::empty);

    for (int local = 0; local < kBatchSize; ++local)
    {
        const auto preset = presetForPatch (firstPatchOfBatch (batchIndex) + local);
        auto& slot = config.slots[static_cast<std::size_t> (preset.index)];
        slot.kind = ihub::SessionSlotKind::plugin;
        slot.path = bundlePath;
        slot.pluginFormat = "VST3";
        slot.pluginName = "TestPlugin";
        slot.instrumentKind = "plugin";
        slot.name = preset.name.toStdString();
        slot.patch = preset.name.toStdString();
        slot.patchName = preset.name.toStdString();
        slot.family = preset.family.toStdString();
        slot.familyName = preset.familyName.toStdString();
    }

    return config;
}

bool buildSession (ihub::Session& session,
                   const ihub::SessionConfig& config,
                   const juce::File& stateDirectory)
{
    session.setPluginStateDirectory (stateDirectory);

    juce::String error;

    if (! session.build (config, error))
    {
        std::cerr << "FAIL batch rig: session build failed: " << error << std::endl;
        return false;
    }

    return true;
}

}
