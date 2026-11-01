#include "Modes.h"

namespace ihub::app
{

AppMode selectMode (const juce::String& commandLine) noexcept
{
    // Headless diagnostics run before any UI: each one prints a report and
    // quits, so they keep priority over the window modes.
    if (commandLine.contains ("--scan-plugins"))
        return AppMode::pluginScan;

    if (commandLine.contains ("--rt-strict"))
        return AppMode::rtStrict;

    if (commandLine.contains ("--list-devices"))
        return AppMode::listDevices;

    if (commandLine.contains ("--measure-latency"))
        return AppMode::measureLatency;

    if (commandLine.contains ("--diagnostics"))
        return AppMode::diagnostics;

    if (commandLine.contains ("--session-smoke"))
        return AppMode::sessionSmoke;

    // UI modes. `serve` starts the API and returns without a window, so it is
    // checked before the modes that create one.
    if (commandLine.contains ("--serve"))
        return AppMode::serve;

    if (commandLine.contains ("--snapshot"))
        return AppMode::snapshot;

    if (commandLine.contains ("--ui-check"))
        return AppMode::uiCheck;

    if (commandLine.contains ("--selftest"))
        return AppMode::selftest;

    return AppMode::gui;
}

const char* toString (AppMode mode) noexcept
{
    switch (mode)
    {
        case AppMode::gui:            return "gui";
        case AppMode::uiCheck:        return "ui-check";
        case AppMode::selftest:       return "selftest";
        case AppMode::serve:          return "serve";
        case AppMode::snapshot:       return "snapshot";
        case AppMode::pluginScan:     return "scan-plugins";
        case AppMode::rtStrict:       return "rt-strict";
        case AppMode::listDevices:    return "list-devices";
        case AppMode::measureLatency: return "measure-latency";
        case AppMode::diagnostics:    return "diagnostics";
        case AppMode::sessionSmoke:   return "session-smoke";
    }

    return "unknown";
}

} // namespace ihub::app
