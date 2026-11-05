#include "Session.h"
#include "concurrent/ConcurrentFixture.h"
#include "concurrent/EditorCapture.h"
#include "concurrent/Reload.h"
#include "shared_page/SharedPageScenario.h"

#include <juce_events/juce_events.h>

#include <iostream>
#include <string>
#include <vector>

#ifndef IHUB_TESTPLUGIN_VST3
#define IHUB_TESTPLUGIN_VST3 ""
#endif

namespace
{
int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "PASS " : "FAIL ") << what << std::endl;

    if (! condition)
        ++failures;
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const juce::String bundlePath (IHUB_TESTPLUGIN_VST3);

    if (bundlePath.isEmpty() || ! juce::File (bundlePath).exists())
    {
        std::cerr << "session-concurrent-instances-test: FAILED, TestPlugin.vst3 is missing"
                  << std::endl;
        return 1;
    }

    const auto tempRoot = juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getChildFile ("ihub-t207-"
                                             + juce::String::toHexString (
                                                 juce::Random::getSystemRandom().nextInt64()));

    (void) tempRoot.deleteRecursively();

    if (! tempRoot.createDirectory())
    {
        std::cerr << "session-concurrent-instances-test: FAILED to create "
                  << tempRoot.getFullPathName() << std::endl;
        return 1;
    }

    const auto wineBundle = ConcurrentInstances::makeWineBundle (
        juce::File (bundlePath), tempRoot);

    if (wineBundle == juce::File() || ! wineBundle.exists())
    {
        std::cerr << "session-concurrent-instances-test: FAILED to create the Wine-style bundle link"
                  << std::endl;
        (void) tempRoot.deleteRecursively();
        return 1;
    }

    const auto stateDirectory = tempRoot.getChildFile ("set.plugin-state");
    const auto config = ConcurrentInstances::makeConfig (wineBundle.getFullPathName());

    std::vector<juce::String> expectedHashes;

    {
        ihub::Session session;
        session.setPluginStateDirectory (stateDirectory);

        juce::String error;

        if (! session.build (config, error))
        {
            std::cerr << "session-concurrent-instances-test: FAILED to build: "
                      << error << std::endl;
            (void) tempRoot.deleteRecursively();
            return 1;
        }

        if (! ConcurrentInstances::waitForAllWarm (session))
        {
            std::cerr << "session-concurrent-instances-test: FAILED, not every slot warmed"
                      << std::endl;
            (void) tempRoot.deleteRecursively();
            return 1;
        }

        check (session.getNumSlots() == ConcurrentInstances::kSlots,
               "the session owns eight plugin slots");

        int processCount = 0;

        for (int slot = 0; slot < ConcurrentInstances::kSlots; ++slot)
            if (session.getSandboxProcessId (slot) > 0)
                ++processCount;

        check (processCount == ConcurrentInstances::kSlots,
               "eight independent sandbox processes host the same plugin");

        ConcurrentInstances::runEditorCapture (session, check);
        expectedHashes = ConcurrentInstances::runReload (session, check);
    }

    {
        ihub::Session session;
        session.setPluginStateDirectory (stateDirectory);

        juce::String error;

        if (! session.build (config, error))
        {
            std::cerr << "session-concurrent-instances-test: FAILED to rebuild: "
                      << error << std::endl;
            (void) tempRoot.deleteRecursively();
            return 1;
        }

        if (! ConcurrentInstances::waitForAllWarm (session))
        {
            std::cerr << "session-concurrent-instances-test: FAILED, not every restored slot warmed"
                      << std::endl;
            (void) tempRoot.deleteRecursively();
            return 1;
        }

        ConcurrentInstances::runRestored (session, expectedHashes, check);
    }

    ConcurrentInstances::runSharedPage (bundlePath, check);

    (void) tempRoot.deleteRecursively();

    if (failures > 0)
    {
        std::cerr << "session-concurrent-instances-test: " << failures
                  << " check(s) failed" << std::endl;
        return 1;
    }

    std::cout << "session-concurrent-instances-test: all checks passed" << std::endl;
    return 0;
}
