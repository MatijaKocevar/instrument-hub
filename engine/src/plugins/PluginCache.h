#pragma once

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <vector>

namespace ihub::plugins
{

/** Persistent per-bundle cache for the out-of-process plugin scanner.

    A full VST3 scan costs one child process per bundle, so the second scan of
    an unchanged directory should not repeat that work. PluginCache stores, for
    every bundle the scanner has seen:

    - a fingerprint of the bundle (recursive size + newest modification time),
    - the plugin descriptions the helper reported, or
    - the reason it could not be scanned.

    A lookup is only a hit when the fingerprint still matches. Copying a new
    version over a bundle changes its fingerprint and therefore invalidates the
    entry automatically; there is no manual "clear cache" step for updates.

    ## Dead-man's-pedal list

    Bundles whose helper crashed or timed out are kept with
    `Outcome::crashed` / `Outcome::timedOut`. `Entry::isBlocked()` is true for
    those, and Scanner skips them on later scans instead of launching the
    helper (and crashing it) again. This is the dead-man's-pedal list: a plugin
    that took the scanner down stays down until

    - the bundle changes (new fingerprint), or
    - the caller explicitly clears the entries with `clearFailures()` /
      `forget()`, or
    - the caller opts into retrying crashed bundles
      (`Scanner::Options::retryCrashedPlugins`).

    Ordinary failures (helper ran, found no plugin, wrong format, ...) are
    recorded but are not blocked: the next scan retries them.

    ## Threading

    Not synchronised. Load, query and record from the scanning thread only.
    The cache is a plain object and is deliberately not touched from the audio
    thread.
*/
class PluginCache final
{
public:
    /** Default cache file name inside the scanner's data directory. */
    static constexpr const char* fileName = "PluginCache.xml";

    /** On-disk schema version. Older / unknown files are ignored. */
    static constexpr int formatVersion = 1;

    enum class Outcome
    {
        success,
        crashed,
        timedOut,
        failed
    };

    /** One bundle, with the state that was observed when it was last scanned. */
    struct Entry
    {
        juce::File file;

        /** Recursive size of the bundle when the entry was recorded. */
        juce::int64 fingerprintSize = 0;

        /** Newest modification time in the bundle (ms since epoch). */
        juce::int64 fingerprintModifiedMs = 0;

        Outcome outcome = Outcome::failed;
        juce::String reason;
        juce::int64 lastAttemptMs = 0;
        std::vector<juce::PluginDescription> plugins;

        /** True for the dead-man's-pedal outcomes: crash or timeout. */
        bool isBlocked() const noexcept
        {
            return outcome == Outcome::crashed || outcome == Outcome::timedOut;
        }

        bool hasPlugins() const noexcept { return ! plugins.empty(); }
    };

    /** Result of a lookup. `fresh` is only true when the bundle fingerprint
        still matches the one stored in the entry. */
    struct Lookup
    {
        bool found = false;
        bool fresh = false;
        Entry entry;
    };

    /** Cheap identity of a bundle used to decide whether a cache entry is
        still valid. Includes every file below a `.vst3` directory, because a
        bundle's directory mtime does not change when a plugin binary is
        overwritten in place. */
    struct Fingerprint
    {
        juce::int64 totalSize = 0;
        juce::int64 modifiedMs = 0;
        juce::int64 fileCount = 0;
    };

    explicit PluginCache (juce::File cacheFile = {});
    ~PluginCache() = default;

    /** Reads the cache from disk. Returns false when the file is missing,
        unreadable, from an unknown schema version or corrupt; in that case the
        in-memory cache is empty and the caller simply scans normally. */
    bool load();

    /** Writes the cache to disk, creating the parent directory if needed. */
    bool save() const;

    /** Drops all entries from memory (does not touch the file until save()). */
    void clear();

    bool isEmpty() const noexcept { return entries.empty(); }

    const juce::File& getCacheFile() const noexcept { return cacheFile; }

    /** Changing the file does not load it; call load() afterwards. */
    void setCacheFile (juce::File newFile);

    int getNumEntries() const noexcept { return static_cast<int> (entries.size()); }

    /** Number of crash/timeout entries currently on the dead-man's-pedal list. */
    int getNumBlockedEntries() const noexcept;

    /** Finds an entry and reports whether its fingerprint is still current. */
    Lookup lookup (const juce::File& bundle) const;

    /** Records a bundle that scanned successfully. An empty plugin list is
        rejected, because caching "success with nothing" would hide a
        disappearing plugin until the bundle changes. */
    bool recordSuccess (const juce::File& bundle,
                        const std::vector<juce::PluginDescription>& plugins);

    /** Records a failed scan. Set `crashed` or `timedOut` to put the bundle on
        the dead-man's-pedal list. `reason` is replayed verbatim on later
        scans. */
    bool recordFailure (const juce::File& bundle,
                        const juce::String& reason,
                        bool crashed,
                        bool timedOut);

    /** Removes one entry, for example to retry a single blocked plugin. */
    bool forget (const juce::File& bundle);

    /** Removes every crash/timeout/failed entry and returns how many were
        removed. Successful entries survive. This is the "release the pedal"
        operation exposed to the UI. */
    int clearFailures();

    /** Removes entries whose bundle no longer exists. Returns the count. */
    int pruneMissing();

    /** All plugin descriptions from fresh successful entries, in scan order.
        Scanner merges these into its KnownPluginList on a cache load. */
    std::vector<juce::PluginDescription> getCachedPlugins() const;

    /** A copy of every crash/timeout entry, for diagnostics and the UI's
        "blocked plugins" list. */
    std::vector<Entry> getBlockedEntries() const;

    static Fingerprint fingerprint (const juce::File& bundle);

    static juce::String outcomeToString (Outcome outcome);
    static Outcome outcomeFromString (const juce::String& text);

private:
    std::vector<Entry>::iterator findEntry (const juce::File& bundle);
    std::vector<Entry>::const_iterator findEntry (const juce::File& bundle) const;

    juce::File cacheFile;
    std::vector<Entry> entries;
};

} // namespace ihub::plugins
