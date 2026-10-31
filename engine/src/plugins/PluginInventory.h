#pragma once

#include "PluginCache.h"
#include "Scanner.h"

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <future>
#include <mutex>
#include <string>
#include <vector>

namespace ihub::plugins
{

/** One installed plug-in as the Plugins page needs it.

    This is the read-only inventory record. Assignment to a patch happens in
    the per-patch instrument/FX endpoints, never here.
*/
struct PluginInfo
{
    /** Stable identity across launches (`PluginDescription::createIdentifierString`). */
    std::string id;
    std::string name;
    std::string description;
    std::string vendor;
    std::string version;
    std::string category;

    /** `true` when the plug-in identifies itself as an instrument. */
    bool isInstrument = false;

    /** `true` for an effect. VST3 types are either one or the other. */
    bool isEffect = true;

    /** Plug-in format, normally `VST3`. */
    std::string format;

    /** Bundle path the scanner reported (`.../Name.vst3`). */
    std::string path;

    static PluginInfo fromDescription (const juce::PluginDescription& description);

    /** Case-insensitive substring match over the metadata and path.
        An empty query matches everything. */
    bool matches (const juce::String& query) const;
};

/** A consistent-enough view of the inventory for one request. */
struct PluginInventorySnapshot
{
    /** Plug-ins matching the requested search, in scan order. */
    std::vector<PluginInfo> plugins;

    /** Number of installed plug-ins before the search filter. */
    int total = 0;

    /** The query the snapshot was filtered with, echoed for the client. */
    std::string query;

    bool scanning = false;
    bool scanned = false;

    /** Epoch milliseconds of the last completed scan / cache load, 0 if never. */
    std::int64_t lastScanMs = 0;

    /** Last scan error; empty on success. */
    std::string error;
};

/** Thread-safe, read-only inventory of installed VST3 plug-ins.

    The heavy lifting stays in `Scanner`: a rescan walks the VST3 directories
    out of process and updates `PluginCache.xml`. This class only projects that
    cache (or, right after a scan, the scanner's report) into the record the
    Plugins page displays. No plugin code runs for `GET /plugins`.

    The persisted cache is loaded in the constructor, so an inventory is
    available immediately after a restart. `requestRescan()` runs one scan on
    a background thread; `snapshot()` and `search` never block on it. Calling
    `requestRescan()` while a scan is running is a no-op that returns false.

    The class is safe to use from HTTP threads. The scanner itself is only
    touched by the background worker.
*/
class PluginInventory final
{
public:
    struct Options
    {
        /** Empty means `Scanner::defaultDataDirectory()`. */
        juce::File dataDirectory;

        /** Empty means `ScannerProcess::findHelperExecutable()`. */
        juce::File helperExecutable;

        /** Empty means `dataDirectory/PluginCache.xml`. */
        juce::File cacheFile;

        /** Empty means the VST3 format's default locations. */
        juce::StringArray searchDirectories;

        int timeoutMs = 20000;
        bool recursive = true;
        bool useCache = true;
        bool retryCrashedPlugins = false;

        /** Optional log hook (UI, tests). When unset, lines go to stderr. */
        std::function<void (const juce::String&)> logSink;
    };

    PluginInventory();
    explicit PluginInventory (Options options);
    ~PluginInventory();

    PluginInventory (const PluginInventory&) = delete;
    PluginInventory& operator= (const PluginInventory&) = delete;

    /** Loads `PluginCache.xml` without launching any plugin process.

        Called by the constructor. Returns false when the file is missing or
        unusable; the inventory is then empty until a rescan finishes.
    */
    bool loadPersisted();

    /** Returns the current inventory, optionally filtered by `search`. */
    PluginInventorySnapshot snapshot (const juce::String& search = {}) const;

    /** Starts a background rescan. Returns false when one is already running. */
    bool requestRescan();

    /** Runs a rescan and waits for it, for tests and offline tools. */
    PluginInventorySnapshot rescanNow (int timeoutMs = 120000);

    /** Blocks until the running scan finishes. Returns false on timeout. */
    bool waitForScan (int timeoutMs = 120000);

    bool isScanning() const;

    const juce::File& getDataDirectory() const noexcept { return options.dataDirectory; }

private:
    void performScan();

    Options options;
    Scanner scanner;
    PluginCache cache;

    mutable std::mutex mutex;
    std::condition_variable scanFinished;
    std::future<void> scanFuture;

    std::vector<PluginInfo> plugins;
    bool scanning = false;
    bool scanned = false;
    std::int64_t lastScanMs = 0;
    std::string lastError;
};

} // namespace ihub::plugins
