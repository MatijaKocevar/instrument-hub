#include "../PluginInventory.h"

#include "PluginInventoryRecords.h"

#include <chrono>
#include <utility>

namespace ihub::plugins
{
bool PluginInventory::requestRescan()
{
    std::future<void> previous;

    {
        const std::lock_guard<std::mutex> lock (mutex);

        if (scanning)
            return false;

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

        if (cache.load())
            descriptions = cache.getCachedPlugins();

        if (descriptions.empty() && ! report.plugins.empty())
            descriptions = report.plugins;

        found = inventory_detail::toPluginInfos (descriptions);

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

}
