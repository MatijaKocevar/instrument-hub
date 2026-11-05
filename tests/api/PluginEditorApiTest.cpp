#include "TestSupport.h"
#include "plugin_editor_api/AnalogLab.h"
#include "plugin_editor_api/Lifecycle.h"
#include "plugin_editor_api/Reload.h"
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
        ApiTest::PluginEditorApi::runReload (rig.session(), rig.client());
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
