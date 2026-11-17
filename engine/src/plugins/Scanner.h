#pragma once

#include "PluginCache.h"
#include "ScannerProcess.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>
#include <vector>

namespace ihub::plugins {
struct ScanFailure {
    juce::File file;
    juce::String reason;
    bool crashed = false;
    bool timedOut = false;
    juce::String helperOutput;
};

struct ScanReport {
    int filesDiscovered = 0;
    int succeeded = 0;
    int failed = 0;

    int cacheHits = 0;

    bool usedCache = false;
    bool wroteCache = false;
    bool helperAvailable = false;
    bool wroteReport = false;
    juce::File reportFile;
    juce::File cacheFile;

    double elapsedMs = 0.0;

    std::vector<juce::PluginDescription> plugins;
    std::vector<ScanFailure> failures;
};

class Scanner {
public:
    struct Options {
        juce::File dataDirectory;

        juce::File helperExecutable;

        int timeoutMs = 20000;
        bool recursive = true;

        bool loadExisting = true;

        bool useCache = true;

        bool retryCrashedPlugins = false;

        juce::File cacheFile;

        std::function<void(const juce::String&)> logSink;
    };

    Scanner();
    explicit Scanner(Options options);

    ScanReport scan(const juce::StringArray& directories);
    ScanReport scan(const juce::FileSearchPath& directories);

    const juce::KnownPluginList& getKnownPlugins() const noexcept { return knownPlugins; }

    const juce::File& getReportFile() const noexcept { return reportFile; }

    static juce::File defaultDataDirectory();
    static juce::File defaultReportFile();

    static constexpr const char* reportFileName = "KnownPluginList.xml";

private:
    void log(const juce::String& message);
    bool loadPersistedTypes();
    juce::StringArray discoverFiles(const juce::StringArray& directories);

    Options options;
    juce::AudioPluginFormatManager formatManager;
    juce::AudioPluginFormat* vst3Format = nullptr;
    juce::KnownPluginList knownPlugins;
    ScannerProcess process;
    PluginCache cache;
    juce::File reportFile;
};
}  // namespace ihub::plugins
