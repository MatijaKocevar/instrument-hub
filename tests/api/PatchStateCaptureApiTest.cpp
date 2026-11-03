#include "TestSupport.h"
#include "patch/Autosave.h"
#include "patch_state_capture_api/Capture.h"
#include "patch_state_capture_api/Fixtures.h"
#include "patch_state_capture_api/Restart.h"

#include <juce_events/juce_events.h>

#include <iostream>

#ifndef IHUB_TESTPLUGIN_VST3
#define IHUB_TESTPLUGIN_VST3 ""
#endif

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    const juce::String bundlePath (IHUB_TESTPLUGIN_VST3);

    if (bundlePath.isEmpty() || ! juce::File (bundlePath).exists())
    {
        std::cerr << "patch-state-capture-api-test: FAILED, TestPlugin.vst3 is missing"
                  << std::endl;
        return 1;
    }

    const auto tempRoot = ApiTest::PatchStateCaptureApi::makeTempRoot();
    const auto stateDirectory = tempRoot.getChildFile ("set.plugin-state");

    (void) tempRoot.deleteRecursively();
    (void) tempRoot.createDirectory();

    const bool captured = ApiTest::PatchStateCaptureApi::runCapture (stateDirectory,
                                                                     bundlePath);

    if (! captured)
    {
        (void) tempRoot.deleteRecursively();
        return 1;
    }

    const bool restarted = ApiTest::PatchStateCaptureApi::runRestart (stateDirectory,
                                                                      bundlePath);

    const bool autosaved = ApiTest::PatchAutosave::runAutosave (tempRoot, bundlePath);

    (void) tempRoot.deleteRecursively();

    if (! restarted || ! autosaved)
        return 1;

    if (ApiTest::failures > 0)
    {
        std::cerr << "patch-state-capture-api-test: " << ApiTest::failures
                  << " check(s) failed" << std::endl;
        return 1;
    }

    std::cout << "patch-state-capture-api-test: all checks passed" << std::endl;
    return 0;
}
