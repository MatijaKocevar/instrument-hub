#pragma once

#include <juce_core/juce_core.h>

namespace ihub::app
{

int runRtStrictCheck (const juce::String& commandLine);

int runPluginScan (const juce::String& commandLine);

int runLatencyMeasurement();

int runListDevices();

int runDiagnostics (const juce::String& commandLine);

int runSessionSmoke (const juce::String& commandLine);

}
