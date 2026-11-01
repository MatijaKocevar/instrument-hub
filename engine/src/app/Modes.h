#pragma once

#include <juce_core/juce_core.h>

namespace ihub::app
{

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

AppMode selectMode (const juce::String& commandLine) noexcept;

const char* toString (AppMode mode) noexcept;

}
