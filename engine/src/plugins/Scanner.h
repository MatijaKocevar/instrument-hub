#pragma once

#include "ScannerProcess.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>
#include <vector>

namespace ihub::plugins
{
struct ScanFailure
{
    juce::File file;
    juce::String reason;
    bool crashed = false;
    bool timedOut = false;
    juce::String helperOutput;
};

struct ScanReport
{
    int filesDiscovered = 0;
    int succeeded = 0;
    int failed = 0;
    bool helperAvailable = false;
    bool wroteReport = false;
    juce::File reportFile;
    std::vector<juce::PluginDescription> plugins;
    std::vector<ScanFailure> failures;
};

/** Scans VST3 directories out of process and persists a KnownPluginList.

    Discovery (walking directories and checking `.vst3`) happens in this
    process and never loads plugin code. Every candidate is then handed to
    ihub-scanner, one child process per bundle, so a plugin that crashes only
    kills the helper. Failures are logged and recorded; good plugins found
    before or after the crash stay in the list.

    The merged list is written as KnownPluginList XML to
    `<dataDirectory>/KnownPluginList.xml` (by default the JUCE user
    application data directory).

    The class is not internally synchronised; call scan() from one thread.
*/
class Scanner
{
public:
    struct Options
    {
        /** Empty means defaultDataDirectory(). */
        juce::File dataDirectory;

        /** Empty means ScannerProcess::findHelperExecutable(). */
        juce::File helperExecutable;

        int timeoutMs = 20000;
        bool recursive = true;

        /** Merge with an existing KnownPluginList.xml instead of starting empty. */
        bool loadExisting = true;

        /** Optional log hook (UI, tests). When unset, lines go to stderr. */
        std::function<void (const juce::String&)> logSink;
    };

    Scanner();
    explicit Scanner (Options options);

    /** Scans the given directories or individual bundles. An empty list uses
        the VST3 format's default locations. */
    ScanReport scan (const juce::StringArray& directories);
    ScanReport scan (const juce::FileSearchPath& directories);

    /** Types known after the last scan, including previously persisted ones. */
    const juce::KnownPluginList& getKnownPlugins() const noexcept { return knownPlugins; }

    /** KnownPluginList XML destination in the user data directory. */
    const juce::File& getReportFile() const noexcept { return reportFile; }

    static juce::File defaultDataDirectory();
    static juce::File defaultReportFile();

    static constexpr const char* reportFileName = "KnownPluginList.xml";

private:
    void log (const juce::String& message);
    bool loadPersistedTypes();
    juce::StringArray discoverFiles (const juce::StringArray& directories);

    Options options;
    juce::AudioPluginFormatManager formatManager;
    juce::AudioPluginFormat* vst3Format = nullptr;
    juce::KnownPluginList knownPlugins;
    ScannerProcess process;
    juce::File reportFile;
};
} // namespace ihub::plugins
