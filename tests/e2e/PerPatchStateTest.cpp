#include "Session.h"
#include "plugins/PluginInstrument.h"

#include <juce_events/juce_events.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#ifndef IHUB_TESTPLUGIN_VST3
#define IHUB_TESTPLUGIN_VST3 ""
#endif

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 128;
constexpr int kSlots = 8;
constexpr int kNeighbourCount = 1;
constexpr int kArmTimeoutMs = 5000;

constexpr float kMarkerA = 0.25f;
constexpr float kMarkerB = 0.75f;
constexpr double kFrequencyA = 220.0;
constexpr double kFrequencyB = 880.0;

constexpr const char* kFamilyIds[] = {
    "keys", "drums", "bass", "pads", "lead", "fx", "perc", "vox"
};

constexpr const char* kFamilyNames[] = {
    "Keys", "Drums", "Bass", "Pads", "Lead", "FX", "Percussion", "Vox"
};

int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

std::string hashOf (const juce::MemoryBlock& data)
{
    std::uint64_t hash = 1469598103934665603ull;
    const auto* bytes = static_cast<const std::uint8_t*> (data.getData());

    for (std::size_t index = 0; index < data.getSize(); ++index)
    {
        hash ^= bytes[index];
        hash *= 1099511628211ull;
    }

    char text[32] {};
    std::snprintf (text, sizeof (text), "%016llx",
                   static_cast<unsigned long long> (hash));
    return text;
}

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

    check (false,
           "slot " + std::to_string (slot) + " did not become active within "
               + std::to_string (kArmTimeoutMs) + " ms (state: "
               + session.getWarmStateName (slot) + ")");
    return false;
}

float normaliseFrequency (double hz)
{
    return static_cast<float> ((hz - 20.0) / (20000.0 - 20.0));
}

bool configureInstrument (ihub::plugins::PluginInstrument& instrument,
                          double frequency,
                          float marker)
{
    bool ok = true;
    ok = instrument.setParameterValue ("Sine Level", 1.0f) && ok;
    ok = instrument.setParameterValue ("Gain", 0.0f) && ok;
    ok = instrument.setParameterValue ("Latency", 0.0f) && ok;
    ok = instrument.setParameterValue ("MIDI Gate", 0.0f) && ok;
    ok = instrument.setParameterValue ("Frequency", normaliseFrequency (frequency)) && ok;
    ok = instrument.setParameterValue ("State Marker", marker) && ok;
    return ok;
}

const juce::MemoryBlock& expectedForSlot (int slot,
                                          const juce::MemoryBlock& stateA,
                                          const juce::MemoryBlock& stateB)
{
    return slot % 2 == 0 ? stateA : stateB;
}

const std::string& expectedHashForSlot (int slot,
                                        const std::string& hashA,
                                        const std::string& hashB)
{
    return slot % 2 == 0 ? hashA : hashB;
}

bool verifyLiveHash (ihub::Session& session,
                     int slot,
                     const juce::MemoryBlock& expected,
                     const std::string& expectedHash,
                     const std::string& phase)
{
    auto* instrument = session.getPluginInstrument (slot);

    if (instrument == nullptr)
    {
        check (false, phase + ": slot " + std::to_string (slot)
                          + " has no live plugin instance");
        return false;
    }

    if (instrument->getStateInformation() != expected)
    {
        check (false, phase + ": slot " + std::to_string (slot)
                          + " bytes differ from the expected state");
        return false;
    }

    const auto liveHash = hashOf (instrument->getStateInformation());
    const bool matches = liveHash == expectedHash;

    check (matches,
           phase + ": slot " + std::to_string (slot) + " live state hash "
               + liveHash + " matches " + expectedHash);

    return matches;
}

ihub::SessionConfig makeConfig (const juce::String& bundlePath)
{
    ihub::SessionConfig config;
    config.device.sampleRate = kSampleRate;
    config.device.bufferSize = kBlockSize;
    config.device.numOutputChannels = 2;
    config.device.numInputChannels = 2;
    config.fadeMilliseconds = 3.0f;
    config.neighbourCount = kNeighbourCount;
    config.initialSlot = 0;

    config.slots = ihub::Session::makeDefaultSet (ihub::SessionSlotKind::empty);
    config.slots.resize (kSlots);

    for (int slot = 0; slot < kSlots; ++slot)
    {
        auto& slotConfig = config.slots[static_cast<std::size_t> (slot)];
        slotConfig.kind = ihub::SessionSlotKind::plugin;
        slotConfig.path = bundlePath;
        slotConfig.pluginFormat = "VST3";
        slotConfig.pluginName = "TestPlugin";
        slotConfig.instrumentKind = "plugin";
        slotConfig.name = std::string (kFamilyNames[slot]) + " Patch";
        slotConfig.patch = std::string (kFamilyIds[slot]) + "-patch";
        slotConfig.patchName = slotConfig.name;
        slotConfig.family = kFamilyIds[slot];
        slotConfig.familyName = kFamilyNames[slot];
    }

    return config;
}

juce::File makeTempRoot()
{
    const juce::File approvedBase ("/tmp/opencode");
    const auto base = approvedBase.isDirectory()
                          ? approvedBase
                          : juce::File::getSpecialLocation (
                                juce::File::tempDirectory);

    return base.getChildFile ("ihub-t131-state-"
                              + juce::String::toHexString (
                                    juce::Random::getSystemRandom().nextInt64()));
}

bool buildSession (ihub::Session& session,
                   const ihub::SessionConfig& config,
                   const juce::File& stateDirectory)
{
    session.setPluginStateDirectory (stateDirectory);

    juce::String error;

    if (! session.build (config, error))
    {
        std::cerr << "per-patch-state-test: build failed: " << error << std::endl;
        return false;
    }

    return true;
}

bool verifySweep (ihub::Session& session,
                  const juce::MemoryBlock& stateA,
                  const juce::MemoryBlock& stateB,
                  const std::string& hashA,
                  const std::string& hashB,
                  const std::string& phase)
{
    bool ok = true;

    for (int slot = 0; slot < kSlots; ++slot)
    {
        if (! armAndWait (session, slot))
        {
            ok = false;
            continue;
        }

        ok = verifyLiveHash (session, slot,
                             expectedForSlot (slot, stateA, stateB),
                             expectedHashForSlot (slot, hashA, hashB),
                             phase)
             && ok;
    }

    return ok;
}

}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const juce::File bundle { juce::String (IHUB_TESTPLUGIN_VST3) };

    if (! bundle.exists())
    {
        std::cerr << "per-patch-state-test: TestPlugin.vst3 is missing ("
                  << bundle.getFullPathName() << ")" << std::endl;
        return 1;
    }

    const auto tempRoot = makeTempRoot();
    const auto stateDirectory = tempRoot.getChildFile ("set.plugin-state");

    (void) tempRoot.deleteRecursively();

    if (! tempRoot.createDirectory().wasOk())
    {
        std::cerr << "per-patch-state-test: could not create "
                  << tempRoot.getFullPathName() << std::endl;
        return 1;
    }

    const auto config = makeConfig (bundle.getFullPathName());

    juce::MemoryBlock stateA;
    juce::MemoryBlock stateB;
    std::string hashA;
    std::string hashB;

    int peakLiveInstances = 0;

    {
        ihub::Session session;

        if (! buildSession (session, config, stateDirectory))
        {
            (void) tempRoot.deleteRecursively();
            return 1;
        }

        if (! armAndWait (session, 0))
        {
            (void) tempRoot.deleteRecursively();
            return 1;
        }

        auto* instrumentA = session.getPluginInstrument (0);
        check (instrumentA != nullptr, "patch 0 has a live TestPlugin instance");

        if (instrumentA == nullptr)
        {
            (void) tempRoot.deleteRecursively();
            return 1;
        }

        check (configureInstrument (*instrumentA, kFrequencyA, kMarkerA),
               "patch 0 accepts state A parameters");
        stateA = instrumentA->getStateInformation();
        hashA = hashOf (stateA);
        check (stateA.getSize() > 0,
               "patch 0 captured state A (" + std::to_string (stateA.getSize())
                   + " bytes, hash " + hashA + ")");
        check (session.capturePluginState (0),
               "patch 0 state A was written to the sidecar");

        if (! armAndWait (session, 1))
        {
            (void) tempRoot.deleteRecursively();
            return 1;
        }

        auto* instrumentB = session.getPluginInstrument (1);
        check (instrumentB != nullptr, "patch 1 has a live TestPlugin instance");

        if (instrumentB == nullptr)
        {
            (void) tempRoot.deleteRecursively();
            return 1;
        }

        check (configureInstrument (*instrumentB, kFrequencyB, kMarkerB),
               "patch 1 accepts state B parameters");
        stateB = instrumentB->getStateInformation();
        hashB = hashOf (stateB);
        check (stateB.getSize() > 0,
               "patch 1 captured state B (" + std::to_string (stateB.getSize())
                   + " bytes, hash " + hashB + ")");
        check (stateA != stateB && hashA != hashB,
               "the two patch states are distinct (" + hashA + " vs " + hashB + ")");
        check (session.capturePluginState (1),
               "patch 1 state B was written to the sidecar");

        for (int slot = 2; slot < kSlots; ++slot)
        {
            if (! armAndWait (session, slot))
                continue;

            auto* instrument = session.getPluginInstrument (slot);

            if (instrument == nullptr)
            {
                check (false, "slot " + std::to_string (slot)
                                  + " has a live plugin instance");
                continue;
            }

            const auto& expected = expectedForSlot (slot, stateA, stateB);

            check (instrument->setStateInformation (expected),
                   "slot " + std::to_string (slot) + " accepted its captured state");
            check (verifyLiveHash (session, slot, expected,
                                   expectedHashForSlot (slot, hashA, hashB),
                                   "capture sweep"),
                   "slot " + std::to_string (slot)
                       + " owns its state during the capture sweep");
            check (session.capturePluginState (slot),
                   "slot " + std::to_string (slot) + " state was written to the sidecar");
        }

        check (! session.isSlotWarm (0) && ! session.isSlotWarm (2),
               "the warm pool evicted the early patches after the capture sweep");
        check (session.getPeakInstrumentCount() <= 1 + kNeighbourCount,
               "live instances stayed within 1 + neighbours (peak "
                   + std::to_string (session.getPeakInstrumentCount()) + ")");

        check (verifySweep (session, stateA, stateB, hashA, hashB, "reload sweep"),
               "the reload sweep restored every state hash after eviction");

        peakLiveInstances = session.getPeakInstrumentCount();

        check (peakLiveInstances <= 1 + kNeighbourCount,
               "the warm-pool instance bound held across evictions and reloads");
        check (session.savePluginStates(), "the sidecar saved explicitly");
        check (session.getPluginStateSize (0) > 0
                   && session.getPluginStateSize (kSlots - 1) > 0,
               "the sidecar holds non-empty blobs for the first and last patch");
    }

    {
        ihub::Session session;

        if (! buildSession (session, config, stateDirectory))
        {
            (void) tempRoot.deleteRecursively();
            return 1;
        }

        check (session.hasPluginState (0) && session.hasPluginState (kSlots - 1),
               "the sidecar is loaded at startup");

        check (verifySweep (session, stateA, stateB, hashA, hashB, "restart sweep"),
               "after the restart every patch restores its own state hash");

        check (session.getPeakInstrumentCount() <= 1 + kNeighbourCount,
               "the restarted pool stayed within 1 + neighbours (peak "
                   + std::to_string (session.getPeakInstrumentCount()) + ")");
    }

    (void) tempRoot.deleteRecursively();

    std::cout << (failures == 0 ? "\nPASS" : "\nFAIL") << ": " << failures
              << " failed check(s)" << std::endl;

    return failures == 0 ? 0 : 1;
}
