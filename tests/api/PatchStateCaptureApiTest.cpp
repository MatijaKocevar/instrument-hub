// ihub-patch-state-capture-api-test: acceptance check for T132.
//
// Drives the Instrument page's explicit save action end to end:
//
//   1. a TestPlugin patch is armed and given a preset A;
//   2. POST /patches/0/state/capture writes the patch sidecar immediately,
//      while the patch is still armed (no switch-away is needed);
//   3. a second preset B is saved and its blob differs from A;
//   4. saving again is an idempotent confirmation with the same stored bytes;
//   5. switching away and back restores B, and a fresh session built from the
//      same sidecar directory also restores B;
//   6. a generated patch and an out-of-range slot are rejected without writing.
//
// Everything is offline and deterministic: rendering is driven by the test and
// plugin creation is bounded by the message-pump deadline used by T131.

#include "TestSupport.h"
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

    (void) tempRoot.deleteRecursively();

    if (! restarted)
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
