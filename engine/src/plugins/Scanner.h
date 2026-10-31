#pragma once

#include "PluginCache.h"
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

    /** How many discovered bundles were answered from PluginCache.xml instead
        of launching a helper. A full cache hit means no plugin code ran. */
    int cacheHits = 0;

    bool usedCache = false;
    bool wroteCache = false;
    bool helperAvailable = false;
    bool wroteReport = false;
    juce::File reportFile;
    juce::File cacheFile;

    /** Wall-clock duration of the whole scan, including discovery. */
    double elapsedMs = 0.0;

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

    Each bundle also gets an entry in `<dataDirectory>/PluginCache.xml`
    (see PluginCache). An unchanged bundle is answered from that cache on later
    scans without launching the helper, and bundles that crashed the helper are
    skipped through the dead-man's-pedal list. Only a changed bundle (new
    fingerprint) or an explicit cache clear triggers another attempt.

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

        /** Reuse PluginCache.xml: unchanged bundles are answered from the
            cache and their plugin code is never loaded. Turn this off to force
            a cold scan (for example from a "rescan anyway" button). */
        bool useCache = true;

        /** Retry bundles previously classified as crashed or timed out. By
            default the dead-man's-pedal list is honoured until the bundle
            changes or the cache entry is cleared explicitly. */
        bool retryCrashedPlugins = false;

        /** Cache destination. Empty means `dataDirectory/PluginCache.xml`. */
        juce::File cacheFile;

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
    PluginCache cache;
    juce::File reportFile;
};
} // namespace ihub::plugins
