#pragma once

#include <juce_core/juce_core.h>

namespace ihub::app
{

/** The single command-line mode the application runs in.

    Diagnostic modes quit as soon as they have printed their report. `serve`
    owns the control API but no window. `snapshot`, `uiCheck` and `selftest`
    open the UI shell and finish from a timer.
*/
enum class AppMode
{
    gui,
    uiCheck,
    selftest,
    serve,
    snapshot,
    pluginScan,
    rtStrict,
    listDevices,
    measureLatency,
    diagnostics,
    sessionSmoke
};

/** Picks the mode from the raw JUCE command line.

    The order is the one the application has always used: a headless
    diagnostic wins over a UI mode, `serve` wins over `snapshot`, and
    `snapshot` wins over `ui-check` and `selftest`.
*/
AppMode selectMode (const juce::String& commandLine) noexcept;

/** Stable name, used by tests and diagnostics. */
const char* toString (AppMode mode) noexcept;

} // namespace ihub::app
