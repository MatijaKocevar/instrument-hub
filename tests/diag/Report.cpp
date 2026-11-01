#include "Report.h"
#include "TestSupport.h"

#include "diag/Diagnostics.h"
#include "diag/LogRing.h"

#include <iostream>

namespace DiagTest
{

void runFormattedReport()
{
    std::cout << "\n-- formatted report --" << std::endl;

    ihub::diag::DiagnosticsSnapshot snapshot;
    snapshot.generatedAt = "2026-10-31T12:00:00";
    snapshot.build.applicationName = "Instrument Hub";
    snapshot.build.version = "0.1.0";
    snapshot.build.buildType = "RelWithDebInfo";
    snapshot.build.juceVersion = "8.0.0";
    snapshot.build.operatingSystem = "Linux";
    snapshot.build.cpuCores = 8;

    snapshot.device.open = true;
    snapshot.device.type = "ALSA";
    snapshot.device.name = "Loopback";
    snapshot.device.sampleRate = 48000.0;
    snapshot.device.bufferSize = 128;
    snapshot.device.bitDepth = 32;
    snapshot.device.inputChannels = 2;
    snapshot.device.outputChannels = 2;
    snapshot.device.inputLatencySamples = 0;
    snapshot.device.outputLatencySamples = 0;
    snapshot.device.deviceXruns = 2;

    snapshot.timing.callbacks = 375;
    snapshot.timing.overruns = 1;
    snapshot.timing.droppedBlocks = 0;
    snapshot.timing.samplesProcessed = 48000;
    snapshot.timing.sampleRate = 48000.0;
    snapshot.timing.blockSize = 128;
    snapshot.timing.deadlineMs = 128.0 * 1000.0 / 48000.0;
    snapshot.timing.lastCallbackMs = 0.5;
    snapshot.timing.rollingMaxCallbackMs = 1.25;
    snapshot.timing.worstCallbackMs = 1.25;
    snapshot.timing.averageCallbackMs = 0.4;
    snapshot.timing.lastLoadPercent = 18.75;
    snapshot.timing.windowSize = 512;

    ihub::diag::PluginDiagnostics loaded;
    loaded.name = "Analog Lab V";
    loaded.manufacturer = "Arturia";
    loaded.version = "5.12";
    loaded.format = "VST3";
    loaded.isInstrument = true;
    loaded.loaded = true;
    loaded.slot = 0;

    ihub::diag::PluginDiagnostics known;
    known.name = "TestPlugin";
    known.format = "VST3";
    known.loaded = false;

    snapshot.plugins = { loaded, known };
    snapshot.knownPluginListFile = juce::File ("/tmp/KnownPluginList.xml");
    snapshot.knownPluginListPresent = true;
    snapshot.crashDumpPath = "/tmp/instrument-hub-crash.log";

    ihub::diag::LogRing log (8);
    log.log (ihub::diag::LogLevel::info,
             ihub::diag::LogSource::diagnostics,
             7,
             "report generated");

    ihub::diag::LogRecord record {};
    log.copyLatest (&record, 1);
    snapshot.recentLog = { record };
    snapshot.logPushed = 1;

    const auto report = ihub::diag::formatDiagnostics (snapshot);

    check (contains (report, "Instrument Hub diagnostics"), "the report has a title");
    check (contains (report, "device: Loopback"), "the device name is printed");
    check (contains (report, "device type: ALSA"), "the device type is printed");
    check (contains (report, "buffer: 128 frames (2.667 ms)"),
           "the buffer size and duration are printed");
    check (contains (report, "latency: input 0 samples"), "input latency is printed");
    check (contains (report, "total 2.667 ms"), "the total latency is printed");
    check (contains (report, "xruns: device 2, callback 1, dropped blocks 0"),
           "device and callback xruns are printed");
    check (contains (report, "callbacks: 375"), "the callback count is printed");
    check (contains (report, "loaded: 1, known: 1"),
           "loaded and known plugin counts are printed");
    check (contains (report, "Analog Lab V") && contains (report, "loaded in slot 0"),
           "the loaded plugin is printed with its slot");
    check (contains (report, "TestPlugin") && contains (report, "known"),
           "the known plugin is printed");
    check (contains (report, "records: 1 of 1"), "the log summary is printed");
    check (contains (report, "report generated"), "the newest log line is printed");
    check (contains (report, "crash dump: /tmp/instrument-hub-crash.log"),
           "the crash dump path is printed");
}

} // namespace DiagTest
