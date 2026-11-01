// ihub-app-modes-test: acceptance support for T137.
//
// The application is now a thin entry point plus a handful of `app/` modules.
// This test pins the two decisions those modules must keep exact:
//
//   * how a command-line value is read (`--option value` and `--option=value`,
//     quoted or not), and
//   * which mode wins when several mode flags are present.
//
// The mode order is the one Main.cpp used before the split: a headless
// diagnostic beats a UI mode, `serve` beats `snapshot`, and `snapshot` beats
// `ui-check`/`selftest`.

#include "app/CommandLine.h"
#include "app/Modes.h"

#include <iostream>

namespace
{
int failures = 0;

void check (bool condition, const juce::String& what)
{
    std::cout << (condition ? "PASS " : "FAIL ") << what << std::endl;

    if (! condition)
        ++failures;
}

void checkValue (const juce::String& commandLine,
                 const juce::String& option,
                 const juce::String& expected)
{
    const auto actual = ihub::app::getCommandLineValue (commandLine, option);
    check (actual == expected,
           "value(" + commandLine + ", " + option + ") == \"" + expected
               + "\" (got \"" + actual + "\")");
}

void checkMode (const juce::String& commandLine, ihub::app::AppMode expected)
{
    const auto actual = ihub::app::selectMode (commandLine);
    check (actual == expected,
           "mode(" + commandLine + ") == " + ihub::app::toString (expected)
               + " (got " + ihub::app::toString (actual) + ")");
}

void testCommandLine()
{
    std::cout << "\n-- command line --" << std::endl;

    checkValue ("--snapshot /tmp/out.png", "--snapshot", "/tmp/out.png");
    checkValue ("--snapshot=/tmp/out.png", "--snapshot", "/tmp/out.png");
    checkValue ("--snapshot \"/tmp/out file.png\"", "--snapshot", "/tmp/out file.png");
    checkValue ("--api-port 18080 --serve", "--api-port", "18080");
    checkValue ("--api-port=18080 --serve", "--api-port", "18080");
    checkValue ("--serve", "--snapshot", "");
    checkValue ("--snapshot", "--snapshot", "");
    checkValue ("--snapshot --serve", "--snapshot", "--serve");
}

void testModes()
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

    // Precedence between combined flags.
    checkMode ("--serve --snapshot /tmp/out.png", ihub::app::AppMode::serve);
    checkMode ("--ui-check --selftest", ihub::app::AppMode::uiCheck);
    checkMode ("--snapshot /tmp/out.png --selftest", ihub::app::AppMode::snapshot);
    checkMode ("--diagnostics --snapshot /tmp/out.png", ihub::app::AppMode::diagnostics);
    checkMode ("--rt-strict --session-smoke", ihub::app::AppMode::rtStrict);

    check (juce::String (ihub::app::toString (ihub::app::AppMode::sessionSmoke))
               == "session-smoke",
           "session-smoke has a stable name");
}
} // namespace

int main()
{
    testCommandLine();
    testModes();

    if (failures > 0)
    {
        std::cerr << "app-modes: " << failures << " check(s) failed" << std::endl;
        return 1;
    }

    std::cout << "app-modes: all checks passed" << std::endl;
    return 0;
}
