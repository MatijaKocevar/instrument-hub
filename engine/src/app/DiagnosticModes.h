#pragma once

#include <juce_core/juce_core.h>

namespace ihub::app {

int runPluginScan(const juce::String& commandLine);

int runLatencyMeasurement();

int runListDevices(const juce::String& commandLine);

int runDiagnostics(const juce::String& commandLine);

}  // namespace ihub::app
