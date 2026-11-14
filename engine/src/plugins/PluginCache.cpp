#include "PluginCache.h"

#include <algorithm>

namespace ihub::plugins
{
    PluginCache::PluginCache (juce::File cacheFileToUse) : cacheFile (std::move (cacheFileToUse))
    {
    }

    void PluginCache::setCacheFile (juce::File newFile)
    {
        cacheFile = std::move (newFile);
    }

    std::vector<PluginCache::Entry>::iterator PluginCache::findEntry (const juce::File& bundle)
    {
        const auto path = bundle.getFullPathName ();

        return std::find_if (entries.begin (),
                             entries.end (),
                             [&path] (const Entry& entry)
                             { return entry.file.getFullPathName () == path; });
    }

    std::vector<PluginCache::Entry>::const_iterator
    PluginCache::findEntry (const juce::File& bundle) const
    {
        const auto path = bundle.getFullPathName ();

        return std::find_if (entries.begin (),
                             entries.end (),
                             [&path] (const Entry& entry)
                             { return entry.file.getFullPathName () == path; });
    }

    PluginCache::Lookup PluginCache::lookup (const juce::File& bundle) const
    {
        Lookup result;

        const auto found = findEntry (bundle);

        if (found == entries.end ())
            return result;

        result.found = true;
        result.entry = *found;

        if (!bundle.exists ())
            return result;

        const auto current = fingerprint (bundle);

        result.fresh = current.totalSize == result.entry.fingerprintSize &&
                       current.modifiedMs == result.entry.fingerprintModifiedMs;

        return result;
    }

    bool PluginCache::recordSuccess (const juce::File& bundle,
                                     const std::vector<juce::PluginDescription>& plugins)
    {
        if (plugins.empty ())
            return false;

        const auto current = fingerprint (bundle);

        if (current.fileCount == 0)
            return false;

        Entry entry;
        entry.file = bundle.getFullPathName ();
        entry.fingerprintSize = current.totalSize;
        entry.fingerprintModifiedMs = current.modifiedMs;
        entry.outcome = Outcome::success;
        entry.lastAttemptMs = juce::Time::currentTimeMillis ();
        entry.plugins = plugins;

        if (const auto existing = findEntry (bundle); existing != entries.end ())
            *existing = std::move (entry);
        else
            entries.push_back (std::move (entry));

        return true;
    }

    bool PluginCache::recordFailure (const juce::File& bundle,
                                     const juce::String& reason,
                                     bool crashed,
                                     bool timedOut)
    {
        const auto current = fingerprint (bundle);

        if (current.fileCount == 0)
            return false;

        Entry entry;
        entry.file = bundle.getFullPathName ();
        entry.fingerprintSize = current.totalSize;
        entry.fingerprintModifiedMs = current.modifiedMs;
        entry.outcome =
            crashed ? Outcome::crashed : (timedOut ? Outcome::timedOut : Outcome::failed);
        entry.reason = reason;
        entry.lastAttemptMs = juce::Time::currentTimeMillis ();

        if (const auto existing = findEntry (bundle); existing != entries.end ())
            *existing = std::move (entry);
        else
            entries.push_back (std::move (entry));

        return true;
    }

    bool PluginCache::forget (const juce::File& bundle)
    {
        const auto existing = findEntry (bundle);

        if (existing == entries.end ())
            return false;

        entries.erase (existing);
        return true;
    }

    int PluginCache::clearFailures ()
    {
        const auto oldSize = entries.size ();

        entries.erase (std::remove_if (entries.begin (),
                                       entries.end (),
                                       [] (const Entry& entry)
                                       { return entry.outcome != Outcome::success; }),
                       entries.end ());

        return static_cast<int> (oldSize - entries.size ());
    }

    int PluginCache::pruneMissing ()
    {
        const auto oldSize = entries.size ();

        entries.erase (std::remove_if (entries.begin (),
                                       entries.end (),
                                       [] (const Entry& entry) { return !entry.file.exists (); }),
                       entries.end ());

        return static_cast<int> (oldSize - entries.size ());
    }

    int PluginCache::getNumBlockedEntries () const noexcept
    {
        return static_cast<int> (std::count_if (entries.begin (),
                                                entries.end (),
                                                [] (const Entry& entry)
                                                { return entry.isBlocked (); }));
    }

    std::vector<juce::PluginDescription> PluginCache::getCachedPlugins () const
    {
        std::vector<juce::PluginDescription> result;

        for (const auto& entry : entries)
        {
            if (entry.outcome != Outcome::success || entry.plugins.empty ())
                continue;

            if (!entry.file.exists ())
                continue;

            const auto current = fingerprint (entry.file);

            if (current.totalSize != entry.fingerprintSize ||
                current.modifiedMs != entry.fingerprintModifiedMs)
                continue;

            result.insert (result.end (), entry.plugins.begin (), entry.plugins.end ());
        }

        return result;
    }

    std::vector<PluginCache::Entry> PluginCache::getBlockedEntries () const
    {
        std::vector<Entry> result;

        for (const auto& entry : entries)
            if (entry.isBlocked ())
                result.push_back (entry);

        return result;
    }

    void PluginCache::clear ()
    {
        entries.clear ();
    }

}
