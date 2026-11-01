#include "../Scanner.h"

namespace ihub::plugins
{
ScanReport Scanner::scan (const juce::StringArray& directories)
{
    const auto startTime = juce::Time::getMillisecondCounterHiRes();

    ScanReport report;
    report.reportFile = reportFile;
    report.cacheFile = cache.getCacheFile();
    report.usedCache = options.useCache;

    if (vst3Format == nullptr)
    {
        log ("scanner: no VST3 format available in this build");
        report.elapsedMs = juce::Time::getMillisecondCounterHiRes() - startTime;
        return report;
    }

    report.helperAvailable = process.getHelperExecutable().existsAsFile();

    if (! report.helperAvailable)
        log ("scanner: helper not found at "
             + process.getHelperExecutable().getFullPathName()
             + "; cached bundles can still be used, uncached ones will fail");

    if (! reportFile.getParentDirectory().createDirectory())
        log ("scanner: could not create " + reportFile.getParentDirectory().getFullPathName());

    if (loadPersistedTypes())
        log ("scanner: loaded " + juce::String (knownPlugins.getNumTypes())
             + " previously known plugin(s) from " + reportFile.getFullPathName());

    if (options.useCache)
    {
        if (cache.load())
            log ("scanner: loaded " + juce::String (cache.getNumEntries())
                 + " cached bundle(s) from " + cache.getCacheFile().getFullPathName());
        else
            log ("scanner: no usable cache at " + cache.getCacheFile().getFullPathName());

        if (const auto blocked = cache.getNumBlockedEntries(); blocked > 0)
            log ("scanner: dead-man's-pedal list holds " + juce::String (blocked)
                 + " bundle(s)");
    }

    const auto files = discoverFiles (directories);
    report.filesDiscovered = files.size();

    for (const auto& path : files)
    {
        const juce::File file (path);

        // Cache lookup first: an unchanged bundle must never launch the
        // helper. That is what makes the second scan of a directory fast and,
        // for a crashing plugin, safe.
        if (options.useCache)
        {
            const auto cached = cache.lookup (file);

            if (cached.found && cached.fresh)
            {
                const auto& entry = cached.entry;

                if (entry.outcome == PluginCache::Outcome::success && entry.hasPlugins())
                {
                    ++report.succeeded;
                    ++report.cacheHits;

                    for (const auto& description : entry.plugins)
                    {
                        knownPlugins.addType (description);
                        report.plugins.push_back (description);
                    }

                    log ("scanner: cache hit: " + path + " ("
                         + juce::String (entry.plugins.size())
                         + " type(s), plugin not loaded)");
                    continue;
                }

                if (entry.isBlocked() && ! options.retryCrashedPlugins)
                {
                    ++report.failed;
                    ++report.cacheHits;

                    ScanFailure failure;
                    failure.file = file;
                    failure.reason = entry.reason.isNotEmpty()
                                         ? entry.reason + " [cached; bundle is on the dead-man's-pedal list]"
                                         : juce::String ("cached crash; bundle is on the dead-man's-pedal list");
                    failure.crashed = entry.outcome == PluginCache::Outcome::crashed;
                    failure.timedOut = entry.outcome == PluginCache::Outcome::timedOut;
                    report.failures.push_back (std::move (failure));

                    log ("scanner: cache: skipping known-"
                         + juce::String (failure.crashed ? "crashing" : "timing-out")
                         + " bundle " + path + " (dead-man's-pedal list)");
                    continue;
                }
            }
        }

        const auto result = process.scanFile (file);

        if (result.succeeded)
        {
            ++report.succeeded;

            for (const auto& description : result.plugins)
            {
                knownPlugins.addType (description);
                report.plugins.push_back (description);
            }

            if (options.useCache)
                cache.recordSuccess (file, result.plugins);

            log ("scanner: ok: " + path + " (" + juce::String (result.plugins.size())
                 + " type(s), " + juce::String (result.elapsedMs, 1) + " ms)");
        }
        else
        {
            ++report.failed;

            ScanFailure failure;
            failure.file = file;
            failure.reason = result.error;
            failure.crashed = result.crashed;
            failure.timedOut = result.timedOut;
            failure.helperOutput = result.output;
            report.failures.push_back (failure);

            // A helper that was never launched says nothing about the plugin
            // (missing binary, scratch directory failure), so it must not
            // poison the cache. A helper that ran and crashed or timed out is
            // exactly what the dead-man's-pedal list is for.
            if (options.useCache && result.launched)
                cache.recordFailure (file, result.error, result.crashed, result.timedOut);

            log ("scanner: failure: " + path + " : " + result.error
                 + " (" + juce::String (result.elapsedMs, 1) + " ms)");
        }
    }

    if (options.useCache)
    {
        if (const auto removed = cache.pruneMissing(); removed > 0)
            log ("scanner: cache: pruned " + juce::String (removed)
                 + " entry/entries for missing bundles");

        report.wroteCache = cache.save();

        if (! report.wroteCache)
            log ("scanner: could not write cache " + cache.getCacheFile().getFullPathName());
    }

    if (const auto xml = knownPlugins.createXml())
        report.wroteReport = xml->writeTo (reportFile);
    else
        log ("scanner: could not serialise KnownPluginList");

    report.elapsedMs = juce::Time::getMillisecondCounterHiRes() - startTime;

    log ("scanner: done: " + juce::String (report.filesDiscovered) + " discovered, "
         + juce::String (report.succeeded) + " ok, " + juce::String (report.failed)
         + " failed, " + juce::String (report.cacheHits) + " cache hit(s), "
         + juce::String (cache.getNumBlockedEntries()) + " blocked, "
         + juce::String (knownPlugins.getNumTypes()) + " plugin(s) known, "
         + juce::String (report.elapsedMs, 1) + " ms"
         + (report.wroteReport ? "; wrote " + reportFile.getFullPathName()
                               : "; report was NOT written")
         + (report.wroteCache ? "; wrote " + cache.getCacheFile().getFullPathName()
                              : ""));

    return report;
}

} // namespace ihub::plugins
