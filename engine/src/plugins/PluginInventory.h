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

namespace ihub::plugins {

struct PluginInfo {
    std::string id;
    std::string name;
    std::string description;
    std::string vendor;
    std::string version;
    std::string category;

    bool isInstrument = false;

    bool isEffect = true;

    std::string format;

    std::string path;

    static PluginInfo fromDescription(const juce::PluginDescription& description);

    bool matches(const juce::String& query) const;
};

struct PluginInventorySnapshot {
    std::vector<PluginInfo> plugins;

    int total = 0;

    std::string query;

    bool scanning = false;
    bool scanned = false;

    std::int64_t lastScanMs = 0;

    std::string error;
};

class PluginInventory final {
public:
    struct Options {
        juce::File dataDirectory;

        juce::File helperExecutable;

        juce::File cacheFile;

        juce::StringArray searchDirectories;

        int timeoutMs = 20000;
        bool recursive = true;
        bool useCache = true;
        bool retryCrashedPlugins = false;

        std::function<void(const juce::String&)> logSink;
    };

    PluginInventory();
    explicit PluginInventory(Options options);
    ~PluginInventory();

    PluginInventory(const PluginInventory&) = delete;
    PluginInventory& operator=(const PluginInventory&) = delete;

    bool loadPersisted();

    PluginInventorySnapshot snapshot(const juce::String& search = {}) const;

    bool requestRescan();

    PluginInventorySnapshot rescanNow(int timeoutMs = 120000);

    bool waitForScan(int timeoutMs = 120000);

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

}  // namespace ihub::plugins
