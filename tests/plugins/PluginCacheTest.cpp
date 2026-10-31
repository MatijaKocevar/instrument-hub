// Acceptance check for T024: plugin cache management.
//
// The task's acceptance criterion is:
//
//     the second scan of the same directory completes in under 100 ms using
//     the cache
//
// The test therefore scans a directory containing TestPlugin.vst3 twice,
// measures only Scanner::scan(), and requires the second scan to be a pure
// cache hit (no helper launch) under the 100 ms budget. It also verifies:
//
//   - the cache file round-trips through PluginCache::load()/save()
//   - a cache-only data directory (no KnownPluginList.xml) still produces the
//     merged plugin list
//   - a crashing bundle lands on the dead-man's-pedal list and is skipped on
//     the next scan without launching the helper again
//   - changing a bundle invalidates its cache entry automatically
//   - a corrupt / future-version cache is ignored rather than trusted
//
// Paths are injected by CMake and may be overridden by argv:
//   argv[1] TestPlugin.vst3, argv[2] CrashingPlugin.vst3, argv[3] ihub-scanner

#include "plugins/PluginCache.h"
#include "plugins/Scanner.h"

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace
{
int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

bool hasTypeNamed (const juce::KnownPluginList& list, const juce::String& name)
{
    for (const auto& type : list.getTypes())
        if (type.name.contains (name))
            return true;

    return false;
}

bool anyLineContains (const std::vector<juce::String>& lines,
                      const juce::String& first,
                      const juce::String& second)
{
    return std::any_of (lines.begin(), lines.end(), [&] (const juce::String& line)
    {
        return line.contains (first) && line.contains (second);
    });
}

struct TimedScan
{
    ihub::plugins::ScanReport report;
    double elapsedMs = 0.0;
};

TimedScan timeScan (ihub::plugins::Scanner& scanner, const juce::StringArray& directories)
{
    TimedScan timed;
    const auto start = juce::Time::getMillisecondCounterHiRes();
    timed.report = scanner.scan (directories);
    timed.elapsedMs = juce::Time::getMillisecondCounterHiRes() - start;

    std::cout << "scan took " << juce::String (timed.elapsedMs, 2)
              << " ms (scanner reported " << juce::String (timed.report.elapsedMs, 2)
              << " ms, " << timed.report.cacheHits << " cache hit(s))" << std::endl;

    return timed;
}

juce::File firstFileBelow (const juce::File& directory)
{
    for (const auto& entry : juce::RangedDirectoryIterator (directory, true, "*",
                                                            juce::File::findFiles))
        return entry.getFile();

    return {};
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const juce::File goodBundle = argc > 1 ? juce::File (juce::String (argv[1]))
                                           : juce::File (IHUB_TESTPLUGIN_VST3);
    const juce::File crashingBundle = argc > 2 ? juce::File (juce::String (argv[2]))
                                               : juce::File (IHUB_CRASHING_VST3);
    const juce::File helper = argc > 3 ? juce::File (juce::String (argv[3]))
                                       : juce::File (IHUB_SCANNER_HELPER);

    std::cout << "test plugin: " << goodBundle.getFullPathName() << std::endl;
    std::cout << "crashing fake: " << crashingBundle.getFullPathName() << std::endl;
    std::cout << "helper: " << helper.getFullPathName() << std::endl;

    check (goodBundle.isDirectory(), "TestPlugin.vst3 bundle exists");
    check (crashingBundle.isDirectory(), "CrashingPlugin.vst3 fake bundle exists");
    check (helper.existsAsFile(), "ihub-scanner helper exists");

    if (failures != 0)
        return 1;

    const auto workspace = juce::File::getSpecialLocation (juce::File::tempDirectory)
                               .getChildFile ("ihub-plugin-cache-acceptance-"
                                              + juce::Uuid().toString());

    workspace.deleteRecursively();
    workspace.createDirectory();

    const auto scanDirectory = workspace.getChildFile ("vst3");
    const auto dataDirectory = workspace.getChildFile ("user-data");
    scanDirectory.createDirectory();
    dataDirectory.createDirectory();

    check (goodBundle.copyDirectoryTo (scanDirectory.getChildFile ("TestPlugin.vst3")),
           "copied TestPlugin.vst3 into the scan directory");

    juce::StringArray directories;
    directories.add (scanDirectory.getFullPathName());

    std::vector<juce::String> logLines;

    ihub::plugins::Scanner::Options options;
    options.dataDirectory = dataDirectory;
    options.helperExecutable = helper;
    options.timeoutMs = 20000;
    options.loadExisting = false;
    options.useCache = true;
    options.logSink = [&logLines] (const juce::String& line)
    {
        logLines.push_back (line);
        std::cout << "log: " << line << std::endl;
    };

    // --- Cold scan: fills the cache ---------------------------------------
    ihub::plugins::Scanner coldScanner (options);
    const auto coldReport = coldScanner.scan (directories);

    std::cout << "cold scan: " << juce::String (coldReport.elapsedMs, 2) << " ms, "
              << coldReport.cacheHits << " cache hit(s)" << std::endl;

    check (coldReport.succeeded == 1, "the cold scan found TestPlugin");
    check (coldReport.failed == 0, "the cold scan had no failures");
    check (coldReport.cacheHits == 0, "the cold scan did not use the cache");
    check (coldReport.wroteCache, "the cold scan wrote the cache");
    check (coldReport.cacheFile == dataDirectory.getChildFile ("PluginCache.xml"),
           "the cache defaults to <data-dir>/PluginCache.xml");

    const auto cacheFile = coldReport.cacheFile;

    check (cacheFile.existsAsFile(), "PluginCache.xml exists after the cold scan");

    {
        ihub::plugins::PluginCache loaded (cacheFile);

        check (loaded.load(), "a fresh PluginCache can load the file");
        check (loaded.getNumEntries() == 1, "the cache holds one bundle");
        check (loaded.getNumBlockedEntries() == 0, "nothing is blocked after a good scan");

        const auto cachedGood = loaded.lookup (scanDirectory.getChildFile ("TestPlugin.vst3"));

        check (cachedGood.found, "the cache has an entry for TestPlugin.vst3");
        check (cachedGood.fresh, "the TestPlugin entry is fresh");
        check (cachedGood.entry.outcome == ihub::plugins::PluginCache::Outcome::success,
               "the TestPlugin entry is a success");
        check (cachedGood.entry.plugins.size() == 1,
               "the TestPlugin entry stores one plugin description");
    }

    // --- Acceptance: second scan is served entirely from the cache --------
    ihub::plugins::Scanner warmScanner (options);
    const auto warm = timeScan (warmScanner, directories);

    check (warm.report.cacheHits == 1, "the second scan reports one cache hit");
    check (warm.report.succeeded == 1, "the second scan still reports TestPlugin");
    check (warm.report.failed == 0, "the second scan has no failures");
    check (warm.report.plugins.size() == 1, "the second scan returns the cached description");
    check (hasTypeNamed (warmScanner.getKnownPlugins(), "TestPlugin"),
           "the cached type is merged into KnownPluginList");
    check (anyLineContains (logLines, "cache hit", "TestPlugin"),
           "a cache-hit line was logged for TestPlugin");
    check (warm.elapsedMs < 100.0,
           "second scan of the same directory completes in under 100 ms (actual "
               + std::to_string (warm.elapsedMs) + " ms)");

    // --- Cache-only data directory: load and merge without a scan ---------
    const auto cacheOnlyDirectory = workspace.getChildFile ("cache-only-data");
    cacheOnlyDirectory.createDirectory();
    check (cacheFile.copyFileTo (cacheOnlyDirectory.getChildFile ("PluginCache.xml")),
           "copied the cache into a data directory with no KnownPluginList.xml");

    ihub::plugins::Scanner::Options cacheOnlyOptions = options;
    cacheOnlyOptions.dataDirectory = cacheOnlyDirectory;

    ihub::plugins::Scanner cacheOnlyScanner (cacheOnlyOptions);
    const auto cacheOnlyReport = cacheOnlyScanner.scan (directories);

    check (cacheOnlyReport.cacheHits == 1, "a cache-only data directory is a cache hit");
    check (cacheOnlyReport.succeeded == 1, "the cache alone yields TestPlugin");
    check (cacheOnlyReport.wroteReport
               && cacheOnlyDirectory.getChildFile ("KnownPluginList.xml").existsAsFile(),
           "the cache-loaded plugins are written to KnownPluginList.xml");
    check (hasTypeNamed (cacheOnlyScanner.getKnownPlugins(), "TestPlugin"),
           "cache load + merge works without a previous KnownPluginList.xml");

    // --- Dead-man's-pedal list --------------------------------------------
    check (crashingBundle.copyDirectoryTo (scanDirectory.getChildFile ("CrashingPlugin.vst3")),
           "copied CrashingPlugin.vst3 into the scan directory");

    ihub::plugins::Scanner crashScanner (options);
    const auto crashReport = crashScanner.scan (directories);

    check (crashReport.filesDiscovered == 2, "the crash scan discovered both bundles");
    check (crashReport.cacheHits == 1, "TestPlugin came from the cache during the crash scan");
    check (crashReport.failed == 1, "the crashing bundle failed once");
    check (crashReport.failures.size() == 1
               && crashReport.failures[0].file.getFileName().contains ("CrashingPlugin"),
           "the failure names CrashingPlugin.vst3");
    check (crashReport.failures.size() == 1 && crashReport.failures[0].crashed,
           "the crashing bundle is classified as a crash");

    {
        ihub::plugins::PluginCache loaded (cacheFile);

        check (loaded.load(), "the cache with a crash entry loads");
        check (loaded.getNumBlockedEntries() == 1,
               "the crashing bundle was added to the dead-man's-pedal list");

        const auto blocked = loaded.lookup (scanDirectory.getChildFile ("CrashingPlugin.vst3"));

        check (blocked.found && blocked.fresh, "the crashed bundle has a fresh cache entry");
        check (blocked.entry.isBlocked(), "the crashed bundle entry is blocked");
        check (blocked.entry.reason.contains ("crashed"),
               "the cached failure reason mentions the crash");
    }

    ihub::plugins::Scanner afterCrashScanner (options);
    const auto afterCrash = timeScan (afterCrashScanner, directories);

    check (afterCrash.report.cacheHits == 2,
           "both bundles were answered from the cache after the crash");
    check (afterCrash.report.failed == 1, "the cached crash is still reported as a failure");
    check (afterCrash.report.failures.size() == 1 && afterCrash.report.failures[0].crashed,
           "the cached failure is still classified as a crash");
    check (afterCrash.report.succeeded == 1, "TestPlugin still scans from the cache");
    check (afterCrash.elapsedMs < 100.0,
           "the crash scan from cache also completes under 100 ms (actual "
               + std::to_string (afterCrash.elapsedMs) + " ms)");
    check (anyLineContains (logLines, "dead-man's-pedal", "CrashingPlugin"),
           "the crash scan logged the dead-man's-pedal skip");

    // Opting into a retry launches the helper and re-records the crash.
    ihub::plugins::Scanner::Options retryOptions = options;
    retryOptions.retryCrashedPlugins = true;

    ihub::plugins::Scanner retryScanner (retryOptions);
    const auto retryReport = retryScanner.scan (directories);

    check (retryReport.cacheHits == 1,
           "only the good plugin is a cache hit when crash retries are enabled");
    check (retryReport.failed == 1, "retrying the crashing plugin fails again");

    // A UI "release the pedal" operation clears the blocked entries.
    {
        ihub::plugins::PluginCache loaded (cacheFile);

        check (loaded.load(), "the cache reloads before clearFailures()");
        check (loaded.getNumBlockedEntries() == 1, "one blocked entry before clearFailures()");
        check (loaded.clearFailures() == 1, "clearFailures() removed the blocked entry");
        check (loaded.getNumBlockedEntries() == 0, "the dead-man's-pedal list is empty");
        check (loaded.getNumEntries() == 1, "the successful entry survived clearFailures()");
        check (loaded.save(), "the cleared cache saves");
    }

    // --- Fingerprint invalidation -----------------------------------------
    const auto testPluginBundle = scanDirectory.getChildFile ("TestPlugin.vst3");
    const auto innerFile = firstFileBelow (testPluginBundle);

    check (innerFile.existsAsFile(), "found a file inside TestPlugin.vst3 for the stamp test");

    if (innerFile.existsAsFile())
    {
        ihub::plugins::PluginCache loaded (cacheFile);

        check (loaded.load(), "the cache loads before the invalidation test");

        const auto before = loaded.lookup (testPluginBundle);
        check (before.found && before.fresh, "TestPlugin is fresh before its file changes");

        innerFile.setLastModificationTime (innerFile.getLastModificationTime()
                                               + juce::RelativeTime::seconds (5));

        const auto after = loaded.lookup (testPluginBundle);
        check (after.found && ! after.fresh,
               "touching a file inside the bundle invalidates the cache entry");
    }

    // --- Corrupt and future-version caches are ignored --------------------
    const auto corruptFile = workspace.getChildFile ("corrupt-cache.xml");
    corruptFile.replaceWithText ("<PLUGINCACHE version=\"1\"><FILE path=");
    {
        ihub::plugins::PluginCache corrupt (corruptFile);
        check (! corrupt.load(), "a malformed cache is rejected");
        check (corrupt.isEmpty(), "a malformed cache leaves the cache empty");
    }

    const auto futureFile = workspace.getChildFile ("future-cache.xml");
    futureFile.replaceWithText ("<PLUGINCACHE version=\"999\"/>");
    {
        ihub::plugins::PluginCache future (futureFile);
        check (! future.load(), "a future schema version is rejected");
        check (future.isEmpty(), "a future schema version leaves the cache empty");
    }

    workspace.deleteRecursively();

    std::cout << (failures == 0 ? "\nPASS" : "\nFAIL") << ": " << failures
              << " failed check(s)" << std::endl;

    return failures == 0 ? 0 : 1;
}
