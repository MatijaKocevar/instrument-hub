#include "Session.h"
#include "editor_follow/EditorAudioScenario.h"
#include "plugins/PluginInstrument.h"
#include "plugins/PluginWindow.h"

#include <juce_events/juce_events.h>

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
constexpr int kSlotA = 0;
constexpr int kSlotB = 2;

int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "PASS " : "FAIL ") << what << std::endl;

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

bool armAndWait (ihub::Session& session, int slot, int timeoutMs = 15000)
{
    session.armSlot (slot);

    const double deadline = juce::Time::getMillisecondCounterHiRes() + timeoutMs;

    while (juce::Time::getMillisecondCounterHiRes() < deadline)
    {
        if (session.getWarmState (slot) == ihub::SessionWarmState::active)
            return true;

        pumpMessageLoop();
    }

    return session.getWarmState (slot) == ihub::SessionWarmState::active;
}

ihub::SessionConfig makeConfig (const juce::String& bundlePath)
{
    ihub::SessionConfig config;
    config.device.sampleRate = kSampleRate;
    config.device.bufferSize = kBlockSize;
    config.device.numOutputChannels = 2;
    config.device.numInputChannels = 2;
    config.neighbourCount = 2;
    config.initialSlot = kSlotA;
    config.slots = ihub::Session::makeDefaultSet (ihub::SessionSlotKind::empty);
    config.slots.resize (kSlots);
    config.slots[1].kind = ihub::SessionSlotKind::initTone;
    config.slots[1].instrumentKind = "init-tone";

    for (const int slot : { kSlotA, kSlotB })
    {
        auto& slotConfig = config.slots[static_cast<std::size_t> (slot)];
        slotConfig.kind = ihub::SessionSlotKind::plugin;
        slotConfig.path = bundlePath;
        slotConfig.pluginFormat = "VST3";
        slotConfig.pluginName = "TestPlugin";
        slotConfig.instrumentKind = "plugin";
        slotConfig.name = slot == kSlotA ? "Editor Patch A" : "Editor Patch B";
        slotConfig.patch = slot == kSlotA ? "editor-a" : "editor-b";
    }

    return config;
}

}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    if (juce::Desktop::getInstance().getDisplays().getPrimaryDisplay() == nullptr)
    {
        std::cerr << "session-editor-follow-test: FAILED, no display (DISPLAY=:0)"
                  << std::endl;
        return 1;
    }

    const juce::String bundlePath (IHUB_TESTPLUGIN_VST3);

    if (bundlePath.isEmpty() || ! juce::File (bundlePath).exists())
    {
        std::cerr << "session-editor-follow-test: FAILED, TestPlugin.vst3 is missing"
                  << std::endl;
        return 1;
    }

    const auto tempRoot = juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getChildFile ("ihub-t130-editor-"
                                             + juce::String::toHexString (
                                                 juce::Random::getSystemRandom().nextInt64()));
    const auto stateDirectory = tempRoot.getChildFile ("set.plugin-state");

    (void) tempRoot.deleteRecursively();
    (void) tempRoot.createDirectory();

    {
        ihub::Session session;
        session.setPluginStateDirectory (stateDirectory);

        juce::String error;

        if (! session.build (makeConfig (bundlePath), error))
        {
            std::cerr << "session-editor-follow-test: FAILED to build: " << error
                      << std::endl;
            return 1;
        }

        check (waitForWarm (session, kSlotA, 15000),
               "patch A's TestPlugin instance is resident");
        check (armAndWait (session, kSlotA), "patch A arms");

        std::string editorError;
        check (session.openEditor (kSlotA, editorError),
               "the armed patch's editor opens" + (editorError.empty()
                                                       ? std::string()
                                                       : " (" + editorError + ")"));
        check (session.isEditorOpen (kSlotA), "the armed patch's editor is tracked");
        check (ihub::plugins::PluginWindow::getOpenWindowCount() == 1,
               "exactly one native window is open");

        EditorAudioScenario::run (session,
                                  kSlotA,
                                  [] (bool condition, const std::string& what)
                                  {
                                      check (condition, what);
                                  });

        if (! waitForWarm (session, kSlotB, 15000))
        {
            session.armSlot (kSlotB);
            (void) waitForWarm (session, kSlotB, 15000);
        }

        check (session.isSlotWarm (kSlotB), "patch B warms");
        check (armAndWait (session, kSlotB), "patch B arms");
        check (! session.isEditorOpen (kSlotA),
               "arming patch B closes patch A's non-armed editor");
        check (ihub::plugins::PluginWindow::getOpenWindowCount() == 0,
               "no editor stays bound to the previous patch");

        check (session.openEditor (kSlotB, editorError),
               "patch B opens its own editor");
        check (session.isEditorOpen (kSlotB),
               "the armed patch's own editor is tracked");

        check (waitForWarm (session, kSlotA, 15000),
               "patch A is still warm for its editor");
        check (session.openEditor (kSlotA, editorError),
               "the non-armed patch's editor can be opened explicitly");
        check (ihub::plugins::PluginWindow::getOpenWindowCount() == 2,
               "both editors are open before the switch");

        check (armAndWait (session, kSlotA), "switching back to patch A arms it");
        check (session.isEditorOpen (kSlotA),
               "the armed patch keeps its own editor across the switch");
        check (! session.isEditorOpen (kSlotB),
               "the non-armed patch's editor is closed by the switch");
        check (ihub::plugins::PluginWindow::getOpenWindowCount() == 1,
               "exactly the armed patch's window survives");

        auto* instrument = session.getPluginInstrument (kSlotA);
        check (instrument != nullptr, "patch A has a live instrument to edit");

        if (instrument != nullptr)
        {
            (void) instrument->setParameterValue ("State Marker", 0.5f);
            (void) instrument->setParameterValue ("Frequency", 0.5f);
        }

        check (session.closeEditor (kSlotA, editorError),
               "closing the armed patch's editor succeeds");
        check (! session.isEditorOpen (kSlotA), "the editor is closed");
        check (ihub::plugins::PluginWindow::getOpenWindowCount() == 0,
               "no native window is left open");

        const auto stateFile = stateDirectory.getChildFile ("slot-0.bin");
        check (session.hasPluginState (kSlotA),
               "closing the editor captured the patch state");
        check (stateFile.existsAsFile() && stateFile.getSize() > 0,
               "the editor-close capture persisted the sidecar blob");

        check (session.openEditor (kSlotA, editorError),
               "the editor reopens after its close");
        session.armSlot (-1);
        check (ihub::plugins::PluginWindow::getOpenWindowCount() == 0,
               "disarming closes every editor");

        const double drainDeadline = juce::Time::getMillisecondCounterHiRes() + 250.0;

        while (juce::Time::getMillisecondCounterHiRes() < drainDeadline)
            pumpMessageLoop();
    }

    (void) tempRoot.deleteRecursively();

    if (failures > 0)
    {
        std::cerr << "session-editor-follow-test: " << failures
                  << " check(s) failed" << std::endl;
        return 1;
    }

    std::cout << "session-editor-follow-test: all checks passed" << std::endl;
    return 0;
}
