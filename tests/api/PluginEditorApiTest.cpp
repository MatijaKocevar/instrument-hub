// ihub-plugin-editor-api-test: acceptance check for T129.
//
// Drives the control API exactly as the Instrument page does and proves the
// native editor lifecycle end to end:
//
//   1. POST /patches/{slot}/editor opens a real native window for TestPlugin;
//   2. a second POST reuses it (one window, no duplicate editor);
//   3. DELETE closes it and is idempotent;
//   4. the window reopens after a close;
//   5. switching the patch source closes the editor before the processor is
//      retired, and the slot can no longer open an editor until a plugin is
//      assigned again;
//   6. a non-plugin patch and an invalid request get a readable error;
//   7. when an Analog Lab V bundle is installed (directly or through
//      yabridge), the same path opens its editor.
//
// The test runs under DISPLAY=:0 like the T023 plugin-window check. JUCE's
// native editor lives on the message thread, so every HTTP request that opens
// or closes a window is issued while the main thread pumps the message loop.

#include "TestSupport.h"
#include "plugin_editor_api/AnalogLab.h"
#include "plugin_editor_api/Lifecycle.h"
#include "plugin_editor_api/SourceChange.h"

#include "RequestDispatch.h"
#include "SessionRig.h"

#include "plugins/PluginWindow.h"

#include <juce_events/juce_events.h>

#include <iostream>

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    if (juce::Desktop::getInstance().getDisplays().getPrimaryDisplay() == nullptr)
    {
        std::cerr << "plugin-editor-api-test: FAILED, no display (DISPLAY=:0)"
                  << std::endl;
        return 1;
    }

    auto config = ApiTest::makeToneConfig (4);
    config.neighbourCount = 1;

    ApiTest::SessionRig rig (config, 30, "plugin-editor-api-test");

    if (! rig.started())
        return 1;

    std::cout << "plugin-editor-api-test: listening on 127.0.0.1:"
              << rig.port() << " slots=" << rig.session().getNumSlots() << std::endl;

    const bool available = ApiTest::PluginEditorApi::runLifecycle (rig.session(),
                                                                   rig.client());

    if (available)
    {
        ApiTest::PluginEditorApi::runSourceChange (rig.session(), rig.client());
        ApiTest::PluginEditorApi::runAnalogLab (rig.session(), rig.client());
    }

    (void) ApiTest::deleteWithDispatch (rig.client(), "/patches/0/editor");
    (void) ApiTest::deleteWithDispatch (rig.client(), "/patches/1/editor");

    ApiTest::check (ihub::plugins::PluginWindow::getOpenWindowCount() == 0,
                    "no native window is left open");

    rig.stop();

    if (! available)
        return 1;

    if (ApiTest::failures > 0)
    {
        std::cerr << "plugin-editor-api-test: " << ApiTest::failures
                  << " check(s) failed" << std::endl;
        std::cerr.flush();
        std::_Exit (1);
    }

    std::cout << "plugin-editor-api-test: all checks passed" << std::endl;
    std::cout.flush();

    std::_Exit (0);
}
