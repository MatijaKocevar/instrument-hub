#include "PluginInventory.h"

#include "scanner/PluginInventoryRecords.h"

#include <utility>

namespace ihub::plugins
{
    namespace
    {
        Scanner::Options makeScannerOptions (const PluginInventory::Options& options)
        {
            Scanner::Options result;
            result.dataDirectory = options.dataDirectory;
            result.helperExecutable = options.helperExecutable;
            result.timeoutMs = options.timeoutMs;
            result.recursive = options.recursive;
            result.loadExisting = true;
            result.useCache = options.useCache;
            result.retryCrashedPlugins = options.retryCrashedPlugins;
            result.cacheFile = options.cacheFile;
            result.logSink = options.logSink;
            return result;
        }
    }

    PluginInventory::PluginInventory () : PluginInventory (Options{})
    {
    }

    PluginInventory::PluginInventory (Options optionsToUse)
        : options (std::move (optionsToUse)), scanner (makeScannerOptions (options))
    {
        if (options.dataDirectory == juce::File ())
            options.dataDirectory = Scanner::defaultDataDirectory ();

        if (options.cacheFile == juce::File ())
            options.cacheFile = options.dataDirectory.getChildFile (PluginCache::fileName);

        cache.setCacheFile (options.cacheFile);

        loadPersisted ();
    }

    PluginInventory::~PluginInventory ()
    {
        std::future<void> pending;

        {
            const std::lock_guard<std::mutex> lock (mutex);
            pending = std::move (scanFuture);
        }

        if (pending.valid ())
            pending.wait ();
    }

    bool PluginInventory::loadPersisted ()
    {
        const bool cacheLoaded = cache.load ();
        auto loaded = cacheLoaded ? inventory_detail::toPluginInfos (cache.getCachedPlugins ())
                                  : std::vector<PluginInfo>{};

        if (loaded.empty ())
            loaded = inventory_detail::pluginsFromReport (options.dataDirectory);

        if (!cacheLoaded && loaded.empty ())
            return false;

        const auto stamp = cache.getCacheFile ().getLastModificationTime ().toMilliseconds ();

        {
            const std::lock_guard<std::mutex> lock (mutex);
            plugins = std::move (loaded);
            scanned = true;
            lastScanMs = stamp;
            lastError.clear ();
        }

        return true;
    }

    PluginInventorySnapshot PluginInventory::snapshot (const juce::String& search) const
    {
        PluginInventorySnapshot result;
        result.query = search.toStdString ();

        const std::lock_guard<std::mutex> lock (mutex);

        result.total = static_cast<int> (plugins.size ());
        result.scanning = scanning;
        result.scanned = scanned;
        result.lastScanMs = lastScanMs;
        result.error = lastError;
        result.plugins.reserve (plugins.size ());

        for (const auto& plugin : plugins)
            if (plugin.matches (search))
                result.plugins.push_back (plugin);

        return result;
    }

}
