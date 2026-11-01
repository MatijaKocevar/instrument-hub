#pragma once

#include <juce_core/juce_core.h>

namespace ihub::app
{

/** `--rt-strict`: renders the engine under RtGuard and reports violations. */
int runRtStrictCheck (const juce::String& commandLine);

/** `--scan-plugins <dir>...`: runs the out-of-process VST3 scanner. */
int runPluginScan (const juce::String& commandLine);

/** `--measure-latency`: measures the loopback round trip five times. */
int runLatencyMeasurement();

/** `--list-devices`: prints the audio device inventory. */
int runListDevices();

/** `--diagnostics`: opens the loopback, renders briefly and prints a report. */
int runDiagnostics (const juce::String& commandLine);

/** `--session-smoke`: renders and streams the default session offline. */
int runSessionSmoke (const juce::String& commandLine);

} // namespace ihub::app
