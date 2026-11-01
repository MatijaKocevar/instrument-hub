#include "Modes.h"
#include "TestSupport.h"

#include <iostream>

namespace AppModesTest
{

void runModes()
{
    std::cout << "\n-- modes --" << std::endl;

    checkMode ("", ihub::app::AppMode::gui);
    checkMode ("--selftest", ihub::app::AppMode::selftest);
    checkMode ("--ui-check", ihub::app::AppMode::uiCheck);
    checkMode ("--snapshot /tmp/out.png", ihub::app::AppMode::snapshot);
    checkMode ("--serve", ihub::app::AppMode::serve);
    checkMode ("--session-smoke", ihub::app::AppMode::sessionSmoke);
    checkMode ("--diagnostics", ihub::app::AppMode::diagnostics);
    checkMode ("--list-devices", ihub::app::AppMode::listDevices);
    checkMode ("--measure-latency", ihub::app::AppMode::measureLatency);
    checkMode ("--rt-strict", ihub::app::AppMode::rtStrict);
    checkMode ("--scan-plugins /tmp/vst3", ihub::app::AppMode::pluginScan);

    checkMode ("--serve --snapshot /tmp/out.png", ihub::app::AppMode::serve);
    checkMode ("--ui-check --selftest", ihub::app::AppMode::uiCheck);
    checkMode ("--snapshot /tmp/out.png --selftest", ihub::app::AppMode::snapshot);
    checkMode ("--diagnostics --snapshot /tmp/out.png", ihub::app::AppMode::diagnostics);
    checkMode ("--rt-strict --session-smoke", ihub::app::AppMode::rtStrict);

    check (juce::String (ihub::app::toString (ihub::app::AppMode::sessionSmoke))
               == "session-smoke",
           "session-smoke has a stable name");
}

}
