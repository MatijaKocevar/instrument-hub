#pragma once

#include "LogRing.h"

#include "audio/DeviceManager.h"
#include "audio/TimingMonitor.h"

#include <juce_core/juce_core.h>

#include <cstdint>
#include <string>
#include <vector>

namespace ihub::diag
{

/** Build and platform identity, stamped into every report. */
struct BuildInfo
{
    std::string applicationName;
    std::string version;
    std::string buildType;
    std::string juceVersion;
    std::string operatingSystem;
    int cpuCores = 0;
};

/** The audio device section of the report. */
struct DeviceDiagnostics
{
    bool open = false;
    std::string type;
    std::string name;

    double sampleRate = 0.0;
    int bufferSize = 0;
    int bitDepth = 0;

    int inputChannels = 0;
    int outputChannels = 0;

    int inputLatencySamples = 0;
    int outputLatencySamples = 0;

    /** Device-reported xruns, not the callback overruns counted by
        `TimingMonitor`. The report prints both. */
    std::uint64_t deviceXruns = 0;
};

/** One plugin, either known to the scanner or loaded into a slot. */
struct PluginDiagnostics
{
    std::string name;
    std::string manufacturer;
    std::string version;
    std::string format;
    std::string file;
    bool isInstrument = false;

    /** True when this instance is loaded into a running engine slot. */
    bool loaded = false;
    int slot = -1;
};

struct DiagnosticsSnapshot
{
    std::string generatedAt;
    BuildInfo build;

    DeviceDiagnostics device;
    AudioEngine::TimingSnapshot timing;

    std::vector<PluginDiagnostics> plugins;
    juce::File knownPluginListFile;
    bool knownPluginListPresent = false;

    std::vector<LogRecord> recentLog;
    std::uint64_t logPushed = 0;
    std::uint64_t logOverwritten = 0;

    std::string crashDumpPath;
};

struct DiagnosticsOptions
{
    /** KnownPluginList.xml produced by the out-of-process scanner. Missing is
        fine: the report says so and the rest is still complete. */
    juce::File knownPluginListFile;

    std::string crashDumpPath;

    /** How many recent log records to include. */
    int maxLogRecords = 80;
};

/** Collects one consistent-enough diagnostics snapshot.

    `loadedPlugins` comes from whoever owns the running instruments; the diag
    module deliberately does not depend on the plugin host. Known (scanned but
    not loaded) plugins are read from `options.knownPluginListFile`.
*/
DiagnosticsSnapshot collectDiagnostics (const AudioEngine::DeviceManager& device,
                                        const AudioEngine::TimingSnapshot& timing,
                                        const std::vector<PluginDiagnostics>& loadedPlugins,
                                        const LogRing& log,
                                        const DiagnosticsOptions& options = {});

/** Parses a KnownPluginList XML file. Returns an empty list on any error. */
std::vector<PluginDiagnostics> loadKnownPlugins (const juce::File& file);

/** Human-readable multi-section report. */
std::string formatDiagnostics (const DiagnosticsSnapshot& snapshot);

/** Writes `formatDiagnostics()` to `file`, creating parent directories. */
bool writeDiagnostics (const DiagnosticsSnapshot& snapshot, const juce::File& file);

/** Routes core `juce::Logger` output into `defaultLog()` so scanner and host
    messages appear in diagnostics and in the crash dump. */
void installJuceLoggerSink();
void uninstallJuceLoggerSink();

} // namespace ihub::diag
