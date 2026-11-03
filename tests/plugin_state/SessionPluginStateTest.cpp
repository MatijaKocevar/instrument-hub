#include "Session.h"
#include "plugins/PluginInstrument.h"

#include <juce_events/juce_events.h>

#include <cmath>
#include <iostream>
#include <string>

#ifndef IHUB_TESTPLUGIN_VST3
#define IHUB_TESTPLUGIN_VST3 ""
#endif

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 128;
constexpr int kSlots = 4;
constexpr int kPluginSlotA = 0;
constexpr int kPluginSlotB = 1;

constexpr double kFrequencyA = 220.0;
constexpr double kFrequencyB = 880.0;
constexpr float kStateMarkerA = 0.25f;
constexpr float kStateMarkerB = 0.75f;

int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

void pumpMessageLoop()
{
   #if JUCE_MODAL_LOOPS_PERMITTED
    if (auto* manager = juce::MessageManager::getInstanceWithoutCreating())
        manager->runDispatchLoopUntil (2);
    else
        juce::Thread::sleep (2);
   #else
    juce::Thread::sleep (2);
   #endif
}

bool waitForWarm (ihub::Session& session, int slot, int timeoutMs)
{
    const double deadline = juce::Time::getMillisecondCounterHiRes() + timeoutMs;

    while (juce::Time::getMillisecondCounterHiRes() < deadline)
    {
        if (session.isSlotWarm (slot))
            return true;

        pumpMessageLoop();
    }

    return session.isSlotWarm (slot);
}

bool armAndWait (ihub::Session& session, int slot, int timeoutMs = 5000)
{
    session.armSlot (slot);
    return session.waitForSlotActive (slot, timeoutMs);
}

float normaliseFrequency (double hz)
{
    return static_cast<float> ((hz - 20.0) / (20000.0 - 20.0));
}

double denormaliseFrequency (float normalised)
{
    return 20.0 + static_cast<double> (normalised) * (20000.0 - 20.0);
}

void applyPreset (ihub::Session& session,
                  int slot,
                  double frequency,
                  float stateMarker)
{
    auto* instrument = session.getPluginInstrument (slot);

    if (instrument == nullptr)
    {
        check (false, "slot " + std::to_string (slot) + " has a live plugin");
        return;
    }

    (void) instrument->setParameterValue ("Sine Level", 1.0f);
    (void) instrument->setParameterValue ("Gain", 0.0f);
    (void) instrument->setParameterValue ("Frequency", normaliseFrequency (frequency));
    (void) instrument->setParameterValue ("State Marker", stateMarker);
}

double measurePitch (ihub::Session& session)
{
    juce::AudioBuffer<float> buffer (2, kBlockSize);
    juce::MidiBuffer midi;

    for (int block = 0; block < 48; ++block)
    {
        buffer.clear();
        session.render (buffer, midi, kBlockSize);
        juce::Thread::sleep (1);
    }

    int crossings = 0;
    int frames = 0;
    float previous = 0.0f;

    for (int block = 0; block < 192; ++block)
    {
        buffer.clear();
        session.render (buffer, midi, kBlockSize);
        juce::Thread::sleep (1);

        for (int frame = 0; frame < kBlockSize; ++frame)
        {
            const float sample = buffer.getSample (0, frame);

            if (previous <= 0.0f && sample > 0.0f)
                ++crossings;

            previous = sample;
            ++frames;
        }
    }

    return frames > 0 ? static_cast<double> (crossings) * kSampleRate
                            / static_cast<double> (frames)
                      : 0.0;
}

bool approximately (double value, double expected, double tolerance)
{
    return std::abs (value - expected) <= tolerance;
}

ihub::SessionConfig makeConfig (const juce::String& bundlePath)
{
    ihub::SessionConfig config;
    config.device.sampleRate = kSampleRate;
    config.device.bufferSize = kBlockSize;
    config.device.numOutputChannels = 2;
    config.device.numInputChannels = 2;
    config.neighbourCount = 1;
    config.initialSlot = kPluginSlotA;
    config.slots = ihub::Session::makeDefaultSet (ihub::SessionSlotKind::empty);
    config.slots.resize (kSlots);

    for (const int slot : { kPluginSlotA, kPluginSlotB })
    {
        auto& slotConfig = config.slots[static_cast<std::size_t> (slot)];
        slotConfig.kind = ihub::SessionSlotKind::plugin;
        slotConfig.path = bundlePath;
        slotConfig.pluginFormat = "VST3";
        slotConfig.pluginName = "TestPlugin";
        slotConfig.instrumentKind = "plugin";
        slotConfig.name = slot == kPluginSlotA ? "Patch A" : "Patch B";
        slotConfig.patch = slot == kPluginSlotA ? "patch-a" : "patch-b";
        slotConfig.patchName = slotConfig.name;
    }

    return config;
}

float stateMarker (ihub::Session& session, int slot)
{
    auto* instrument = session.getPluginInstrument (slot);
    return instrument != nullptr ? instrument->getParameterValue ("State Marker") : -1.0f;
}

}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const juce::String bundlePath (IHUB_TESTPLUGIN_VST3);

    if (bundlePath.isEmpty() || ! juce::File (bundlePath).exists())
    {
        std::cerr << "session-plugin-state-test: FAILED, TestPlugin.vst3 is missing"
                  << std::endl;
        return 1;
    }

    const auto tempRoot = juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getChildFile ("ihub-t130-"
                                             + juce::String::toHexString (
                                                 juce::Random::getSystemRandom().nextInt64()));
    const auto stateDirectory = tempRoot.getChildFile ("set.plugin-state");

    (void) tempRoot.deleteRecursively();
    (void) tempRoot.createDirectory();

    const auto config = makeConfig (bundlePath);

    juce::MemoryBlock seededState;

    {
        ihub::Session session;
        session.setPluginStateDirectory (stateDirectory);

        juce::String error;

        if (! session.build (config, error))
        {
            std::cerr << "session-plugin-state-test: FAILED to build: " << error
                      << std::endl;
            return 1;
        }

        check (waitForWarm (session, kPluginSlotA, 15000),
               "patch A's TestPlugin instance is resident");
        check (armAndWait (session, kPluginSlotA),
               "patch A arms");

        applyPreset (session, kPluginSlotA, kFrequencyA, kStateMarkerA);

        if (auto* instrument = session.getPluginInstrument (kPluginSlotA))
            seededState = instrument->getStateInformation();

        check (seededState.getSize() > 0, "patch A produced a seedable state");

        check (waitForWarm (session, kPluginSlotB, 15000),
               "patch B's TestPlugin instance is resident");
        check (armAndWait (session, kPluginSlotB),
               "switching to patch B arms it");
        applyPreset (session, kPluginSlotB, kFrequencyB, kStateMarkerB);

        check (approximately (measurePitch (session), kFrequencyB, 40.0),
               "patch B renders its own pitch before the restart");

        check (armAndWait (session, kPluginSlotA),
               "switching back to patch A arms it");

        const auto pitchA = measurePitch (session);
        check (approximately (pitchA, kFrequencyA, 15.0),
               "patch A still renders its own pitch after the round trip ("
                   + std::to_string (pitchA) + " Hz)");
        check (approximately (stateMarker (session, kPluginSlotA), kStateMarkerA, 0.01),
               "patch A keeps its own state marker");

        const auto fileA = stateDirectory.getChildFile ("slot-0.bin");
        const auto fileB = stateDirectory.getChildFile ("slot-1.bin");

        check (fileA.existsAsFile() && fileA.getSize() > 0,
               "switching away persisted patch A's blob");
        check (fileB.existsAsFile() && fileB.getSize() > 0,
               "switching away persisted patch B's blob");

        juce::MemoryBlock storedA;
        juce::MemoryBlock storedB;
        (void) fileA.loadFileAsData (storedA);
        (void) fileB.loadFileAsData (storedB);

        check (storedA != storedB,
               "the two patches have different stored blobs");

        check (session.savePluginStates(), "an explicit save writes the sidecar");
    }

    {
        ihub::Session session;
        session.setPluginStateDirectory (stateDirectory);

        check (session.hasPluginState (kPluginSlotA)
                   && session.hasPluginState (kPluginSlotB),
               "the sidecar is loaded at startup");
        check (session.getPluginStateSize (kPluginSlotA) > 0
                   && session.getPluginStateSize (kPluginSlotB) > 0,
               "both patches have a non-empty restored blob");

        juce::String error;

        if (! session.build (config, error))
        {
            std::cerr << "session-plugin-state-test: FAILED to rebuild: " << error
                      << std::endl;
            return 1;
        }

        check (waitForWarm (session, kPluginSlotA, 15000),
               "patch A warms after the restart");
        check (armAndWait (session, kPluginSlotA), "patch A arms after the restart");

        const auto restoredA = stateMarker (session, kPluginSlotA);
        check (approximately (restoredA, kStateMarkerA, 0.01),
               "patch A restores its own state marker (" + std::to_string (restoredA)
                   + ")");

        auto* instrumentA = session.getPluginInstrument (kPluginSlotA);
        const auto frequencyA = instrumentA != nullptr
                                    ? denormaliseFrequency (
                                          instrumentA->getParameterValue ("Frequency"))
                                    : 0.0;
        check (approximately (frequencyA, kFrequencyA, 5.0),
               "patch A restores its own frequency (" + std::to_string (frequencyA)
                   + " Hz)");

        const auto pitchAfterRestartA = measurePitch (session);
        check (approximately (pitchAfterRestartA, kFrequencyA, 15.0),
               "patch A renders its own sound after the restart ("
                   + std::to_string (pitchAfterRestartA) + " Hz)");

        check (armAndWait (session, kPluginSlotB), "patch B arms after the restart");

        const auto restoredB = stateMarker (session, kPluginSlotB);
        check (approximately (restoredB, kStateMarkerB, 0.01),
               "patch B restores its own state marker (" + std::to_string (restoredB)
                   + ")");

        auto* instrumentB = session.getPluginInstrument (kPluginSlotB);
        const auto frequencyB = instrumentB != nullptr
                                    ? denormaliseFrequency (
                                          instrumentB->getParameterValue ("Frequency"))
                                    : 0.0;
        check (approximately (frequencyB, kFrequencyB, 5.0),
               "patch B restores its own frequency (" + std::to_string (frequencyB)
                   + " Hz)");

        const auto pitchAfterRestartB = measurePitch (session);
        check (approximately (pitchAfterRestartB, kFrequencyB, 40.0),
               "patch B renders its own sound after the restart ("
                   + std::to_string (pitchAfterRestartB) + " Hz)");
    }

    {
        auto seededConfig = makeConfig (bundlePath);
        seededConfig.slots[static_cast<std::size_t> (kPluginSlotA)].pluginState = seededState;

        ihub::Session session;
        juce::String error;

        if (! session.build (seededConfig, error))
        {
            std::cerr << "session-plugin-state-test: FAILED to seed: " << error
                      << std::endl;
            return 1;
        }

        check (waitForWarm (session, kPluginSlotA, 15000),
               "the seeded patch warms");
        check (armAndWait (session, kPluginSlotA), "the seeded patch arms");

        const auto marker = stateMarker (session, kPluginSlotA);
        check (approximately (marker, kStateMarkerA, 0.01),
               "a project-seeded state reaches the plugin ("
                   + std::to_string (marker) + ")");

        auto* instrument = session.getPluginInstrument (kPluginSlotA);
        const auto frequency = instrument != nullptr
                                    ? denormaliseFrequency (
                                          instrument->getParameterValue ("Frequency"))
                                    : 0.0;
        check (approximately (frequency, kFrequencyA, 5.0),
               "the seeded state restores the patch frequency ("
                   + std::to_string (frequency) + " Hz)");
    }

    (void) tempRoot.deleteRecursively();

    if (failures > 0)
    {
        std::cerr << "session-plugin-state-test: " << failures
                  << " check(s) failed" << std::endl;
        return 1;
    }

    std::cout << "session-plugin-state-test: all checks passed" << std::endl;
    return 0;
}
