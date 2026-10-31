#include "PluginInventory.h"

#include <algorithm>
#include <chrono>
#include <set>
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

std::string keyFor (const PluginInfo& info)
{
    return info.path + "|" + info.id;
}

std::vector<PluginInfo> toPluginInfos (const std::vector<juce::PluginDescription>& descriptions)
{
    std::vector<PluginInfo> result;
    std::set<std::string> seen;

    result.reserve (descriptions.size());

    for (const auto& description : descriptions)
    {
        auto info = PluginInfo::fromDescription (description);

        if (info.name.empty() && info.path.empty())
            continue;

        if (seen.insert (keyFor (info)).second)
            result.push_back (std::move (info));
    }

    return result;
}

/** Legacy fallback: a KnownPluginList.xml without a cache entry.

    Only bundles that still exist are kept, so a removed bundle does not
    reappear just because it is still in the old report.
*/
std::vector<PluginInfo> pluginsFromReport (const juce::File& dataDirectory)
{
    const auto report = dataDirectory.getChildFile (Scanner::reportFileName);

    if (! report.existsAsFile())
        return {};

    const auto xml = juce::XmlDocument::parse (report);

    if (xml == nullptr)
        return {};

    juce::KnownPluginList list;
    list.recreateFromXml (*xml);

    std::vector<juce::PluginDescription> existing;

    for (const auto& description : list.getTypes())
        if (juce::File (description.fileOrIdentifier).exists())
            existing.push_back (description);

    return toPluginInfos (existing);
}
} // namespace

PluginInfo PluginInfo::fromDescription (const juce::PluginDescription& description)
{
    PluginInfo info;

    info.id = description.createIdentifierString().toStdString();
    info.name = description.name.toStdString();
    info.description = description.descriptiveName.toStdString();
    info.vendor = description.manufacturerName.toStdString();
    info.version = description.version.toStdString();
    info.category = description.category.toStdString();
    info.format = description.pluginFormatName.toStdString();
    info.path = description.fileOrIdentifier.toStdString();
    info.isInstrument = description.isInstrument;
    info.isEffect = ! description.isInstrument;

    if (info.format.empty())
        info.format = "VST3";

    if (info.category.empty())
        info.category = info.isInstrument ? "Instrument" : "Fx";

    return info;
}

bool PluginInfo::matches (const juce::String& query) const
{
    const auto needle = query.trim();

    if (needle.isEmpty())
        return true;

    const auto matchesText = [&needle] (const std::string& text)
    {
        return juce::String (text).containsIgnoreCase (needle);
    };

    return matchesText (name)
           || matchesText (vendor)
           || matchesText (version)
           || matchesText (category)
           || matchesText (description)
           || matchesText (format)
           || matchesText (path)
           || matchesText (id);
}

PluginInventory::PluginInventory()
    : PluginInventory (Options {})
{
}

PluginInventory::PluginInventory (Options optionsToUse)
    : options (std::move (optionsToUse)),
      scanner (makeScannerOptions (options))
{
    if (options.dataDirectory == juce::File())
        options.dataDirectory = Scanner::defaultDataDirectory();

    if (options.cacheFile == juce::File())
        options.cacheFile = options.dataDirectory.getChildFile (PluginCache::fileName);

    cache.setCacheFile (options.cacheFile);

    loadPersisted();
}

PluginInventory::~PluginInventory()
{
    std::future<void> pending;

    {
        const std::lock_guard<std::mutex> lock (mutex);
        pending = std::move (scanFuture);
    }

    if (pending.valid())
        pending.wait();
}

bool PluginInventory::loadPersisted()
{
    const bool cacheLoaded = cache.load();
    auto loaded = cacheLoaded ? toPluginInfos (cache.getCachedPlugins())
                              : std::vector<PluginInfo> {};

    if (loaded.empty())
        loaded = pluginsFromReport (options.dataDirectory);

    if (! cacheLoaded && loaded.empty())
        return false;

    const auto stamp = cache.getCacheFile().getLastModificationTime().toMilliseconds();

    {
        const std::lock_guard<std::mutex> lock (mutex);
        plugins = std::move (loaded);
        scanned = true;
        lastScanMs = stamp;
        lastError.clear();
    }

    return true;
}

PluginInventorySnapshot PluginInventory::snapshot (const juce::String& search) const
{
    PluginInventorySnapshot result;
    result.query = search.toStdString();

    const std::lock_guard<std::mutex> lock (mutex);

    result.total = static_cast<int> (plugins.size());
    result.scanning = scanning;
    result.scanned = scanned;
    result.lastScanMs = lastScanMs;
    result.error = lastError;
    result.plugins.reserve (plugins.size());

    for (const auto& plugin : plugins)
        if (plugin.matches (search))
            result.plugins.push_back (plugin);

    return result;
}

bool PluginInventory::requestRescan()
{
    std::future<void> previous;

    {
        const std::lock_guard<std::mutex> lock (mutex);

        if (scanning)
            return false;

        // The previous future is finished (otherwise `scanning` would be set)
        // but may still need reaping before a new thread replaces it.
        if (scanFuture.valid())
            previous = std::move (scanFuture);

        scanning = true;
        lastError.clear();
    }

    if (previous.valid())
        previous.wait();

    try
    {
        const std::lock_guard<std::mutex> lock (mutex);
        scanFuture = std::async (std::launch::async, [this] { performScan(); });
    }
    catch (...)
    {
        const std::lock_guard<std::mutex> lock (mutex);
        scanning = false;
        lastError = "could not start the plugin scan";
        scanFinished.notify_all();
        return false;
    }

    return true;
}

bool PluginInventory::waitForScan (int timeoutMs)
{
    std::unique_lock<std::mutex> lock (mutex);

    if (! scanning)
        return true;

    return scanFinished.wait_for (lock,
                                  std::chrono::milliseconds (juce::jmax (1, timeoutMs)),
                                  [this] { return ! scanning; });
}

PluginInventorySnapshot PluginInventory::rescanNow (int timeoutMs)
{
    (void) requestRescan();
    (void) waitForScan (timeoutMs);
    return snapshot();
}

bool PluginInventory::isScanning() const
{
    const std::lock_guard<std::mutex> lock (mutex);
    return scanning;
}

void PluginInventory::performScan()
{
    std::vector<PluginInfo> found;
    std::string error;

    try
    {
        const auto report = scanner.scan (options.searchDirectories);

        std::vector<juce::PluginDescription> descriptions;

        // The cache is the authoritative "currently installed" set: it drops
        // entries for missing bundles and only holds fresh fingerprints. The
        // scan report is the fallback if the cache could not be reread.
        if (cache.load())
            descriptions = cache.getCachedPlugins();

        if (descriptions.empty() && ! report.plugins.empty())
            descriptions = report.plugins;

        found = toPluginInfos (descriptions);

        if (report.failed > 0)
            error = std::to_string (report.failed) + " plug-in bundle(s) failed to scan";
    }
    catch (const std::exception& exception)
    {
        error = std::string ("plugin scan failed: ") + exception.what();
    }
    catch (...)
    {
        error = "plugin scan failed";
    }

    {
        const std::lock_guard<std::mutex> lock (mutex);
        plugins = std::move (found);
        scanning = false;
        scanned = true;
        lastScanMs = juce::Time::currentTimeMillis();
        lastError = std::move (error);
    }

    scanFinished.notify_all();
}

} // namespace ihub::plugins
