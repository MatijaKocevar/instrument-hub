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

    struct BuildInfo
    {
        std::string applicationName;
        std::string version;
        std::string buildType;
        std::string juceVersion;
        std::string operatingSystem;
        int cpuCores = 0;
    };

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

        std::uint64_t deviceXruns = 0;
    };

    struct PluginDiagnostics
    {
        std::string name;
        std::string manufacturer;
        std::string version;
        std::string format;
        std::string file;
        bool isInstrument = false;

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
        juce::File knownPluginListFile;

        std::string crashDumpPath;

        int maxLogRecords = 80;
    };

    DiagnosticsSnapshot collectDiagnostics (const AudioEngine::DeviceManager& device,
                                            const AudioEngine::TimingSnapshot& timing,
                                            const std::vector<PluginDiagnostics>& loadedPlugins,
                                            const LogRing& log,
                                            const DiagnosticsOptions& options = {});

    std::vector<PluginDiagnostics> loadKnownPlugins (const juce::File& file);

    std::string formatDiagnostics (const DiagnosticsSnapshot& snapshot);

    bool writeDiagnostics (const DiagnosticsSnapshot& snapshot, const juce::File& file);

    void installJuceLoggerSink ();
    void uninstallJuceLoggerSink ();

}
