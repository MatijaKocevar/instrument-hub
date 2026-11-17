#pragma once

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <vector>

namespace ihub::plugins {

class PluginCache final {
public:
    static constexpr const char* fileName = "PluginCache.xml";

    static constexpr int formatVersion = 1;

    enum class Outcome { success, crashed, timedOut, failed };

    struct Entry {
        juce::File file;

        juce::int64 fingerprintSize = 0;

        juce::int64 fingerprintModifiedMs = 0;

        Outcome outcome = Outcome::failed;
        juce::String reason;
        juce::int64 lastAttemptMs = 0;
        std::vector<juce::PluginDescription> plugins;

        bool isBlocked() const noexcept {
            return outcome == Outcome::crashed || outcome == Outcome::timedOut;
        }

        bool hasPlugins() const noexcept { return !plugins.empty(); }
    };

    struct Lookup {
        bool found = false;
        bool fresh = false;
        Entry entry;
    };

    struct Fingerprint {
        juce::int64 totalSize = 0;
        juce::int64 modifiedMs = 0;
        juce::int64 fileCount = 0;
    };

    explicit PluginCache(juce::File cacheFile = {});
    ~PluginCache() = default;

    bool load();

    bool save() const;

    void clear();

    bool isEmpty() const noexcept { return entries.empty(); }

    const juce::File& getCacheFile() const noexcept { return cacheFile; }

    void setCacheFile(juce::File newFile);

    int getNumEntries() const noexcept { return static_cast<int>(entries.size()); }

    int getNumBlockedEntries() const noexcept;

    Lookup lookup(const juce::File& bundle) const;

    bool recordSuccess(const juce::File& bundle,
                       const std::vector<juce::PluginDescription>& plugins);

    bool recordFailure(const juce::File& bundle, const juce::String& reason, bool crashed,
                       bool timedOut);

    bool forget(const juce::File& bundle);

    int clearFailures();

    int pruneMissing();

    std::vector<juce::PluginDescription> getCachedPlugins() const;

    std::vector<Entry> getBlockedEntries() const;

    static Fingerprint fingerprint(const juce::File& bundle);

    static juce::String outcomeToString(Outcome outcome);
    static Outcome outcomeFromString(const juce::String& text);

private:
    std::vector<Entry>::iterator findEntry(const juce::File& bundle);
    std::vector<Entry>::const_iterator findEntry(const juce::File& bundle) const;

    juce::File cacheFile;
    std::vector<Entry> entries;
};

}  // namespace ihub::plugins
