#pragma once

#include <juce_core/juce_core.h>

namespace ihub::app {

enum class AppMode { gui, serve, pluginScan, listDevices, measureLatency, diagnostics };

AppMode selectMode(const juce::String& commandLine) noexcept;

const char* toString(AppMode mode) noexcept;

}  // namespace ihub::app
