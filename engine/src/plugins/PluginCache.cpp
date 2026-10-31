#include "PluginCache.h"

#include <algorithm>

namespace ihub::plugins
{
namespace
{
constexpr const char* rootTag = "PLUGINCACHE";
constexpr const char* entryTag = "FILE";

juce::String makeFingerprintAttribute (juce::int64 value)
{
    return juce::String (value);
}
} // namespace

PluginCache::PluginCache (juce::File cacheFileToUse)
    : cacheFile (std::move (cacheFileToUse))
{
}

void PluginCache::setCacheFile (juce::File newFile)
{
    cacheFile = std::move (newFile);
}

PluginCache::Fingerprint PluginCache::fingerprint (const juce::File& bundle)
{
    Fingerprint result;

    if (! bundle.exists())
        return result;

    const auto rootModification = bundle.getLastModificationTime().toMilliseconds();

    result.modifiedMs = rootModification;
    ++result.fileCount;

    if (bundle.isDirectory())
    {
        for (const auto& entry : juce::RangedDirectoryIterator (bundle, true, "*",
                                                                juce::File::findFiles))
        {
            const auto size = entry.getFileSize();

            if (size > 0)
                result.totalSize += size;

            const auto modified = entry.getModificationTime().toMilliseconds();

            if (modified > result.modifiedMs)
                result.modifiedMs = modified;

            ++result.fileCount;
        }
    }
    else
    {
        const auto size = bundle.getSize();

        if (size > 0)
            result.totalSize = size;
    }

    return result;
}

std::vector<PluginCache::Entry>::iterator PluginCache::findEntry (const juce::File& bundle)
{
    const auto path = bundle.getFullPathName();

    return std::find_if (entries.begin(), entries.end(),
                         [&path] (const Entry& entry)
                         {
                             return entry.file.getFullPathName() == path;
                         });
}

std::vector<PluginCache::Entry>::const_iterator PluginCache::findEntry (const juce::File& bundle) const
{
    const auto path = bundle.getFullPathName();

    return std::find_if (entries.begin(), entries.end(),
                         [&path] (const Entry& entry)
                         {
                             return entry.file.getFullPathName() == path;
                         });
}

PluginCache::Lookup PluginCache::lookup (const juce::File& bundle) const
{
    Lookup result;

    const auto found = findEntry (bundle);

    if (found == entries.end())
        return result;

    result.found = true;
    result.entry = *found;

    if (! bundle.exists())
        return result;

    const auto current = fingerprint (bundle);

    result.fresh = current.totalSize == result.entry.fingerprintSize
                   && current.modifiedMs == result.entry.fingerprintModifiedMs;

    return result;
}

bool PluginCache::recordSuccess (const juce::File& bundle,
                                 const std::vector<juce::PluginDescription>& plugins)
{
    if (plugins.empty())
        return false;

    const auto current = fingerprint (bundle);

    if (current.fileCount == 0)
        return false;

    Entry entry;
    entry.file = bundle.getFullPathName();
    entry.fingerprintSize = current.totalSize;
    entry.fingerprintModifiedMs = current.modifiedMs;
    entry.outcome = Outcome::success;
    entry.lastAttemptMs = juce::Time::currentTimeMillis();
    entry.plugins = plugins;

    if (const auto existing = findEntry (bundle); existing != entries.end())
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
    entry.file = bundle.getFullPathName();
    entry.fingerprintSize = current.totalSize;
    entry.fingerprintModifiedMs = current.modifiedMs;
    entry.outcome = crashed ? Outcome::crashed
                            : (timedOut ? Outcome::timedOut : Outcome::failed);
    entry.reason = reason;
    entry.lastAttemptMs = juce::Time::currentTimeMillis();

    if (const auto existing = findEntry (bundle); existing != entries.end())
        *existing = std::move (entry);
    else
        entries.push_back (std::move (entry));

    return true;
}

bool PluginCache::forget (const juce::File& bundle)
{
    const auto existing = findEntry (bundle);

    if (existing == entries.end())
        return false;

    entries.erase (existing);
    return true;
}

int PluginCache::clearFailures()
{
    const auto oldSize = entries.size();

    entries.erase (std::remove_if (entries.begin(), entries.end(),
                                   [] (const Entry& entry)
                                   {
                                       return entry.outcome != Outcome::success;
                                   }),
                   entries.end());

    return static_cast<int> (oldSize - entries.size());
}

int PluginCache::pruneMissing()
{
    const auto oldSize = entries.size();

    entries.erase (std::remove_if (entries.begin(), entries.end(),
                                   [] (const Entry& entry)
                                   {
                                       return ! entry.file.exists();
                                   }),
                   entries.end());

    return static_cast<int> (oldSize - entries.size());
}

int PluginCache::getNumBlockedEntries() const noexcept
{
    return static_cast<int> (std::count_if (entries.begin(), entries.end(),
                                            [] (const Entry& entry)
                                            {
                                                return entry.isBlocked();
                                            }));
}

std::vector<juce::PluginDescription> PluginCache::getCachedPlugins() const
{
    std::vector<juce::PluginDescription> result;

    for (const auto& entry : entries)
    {
        if (entry.outcome != Outcome::success || entry.plugins.empty())
            continue;

        if (! entry.file.exists())
            continue;

        const auto current = fingerprint (entry.file);

        if (current.totalSize != entry.fingerprintSize
            || current.modifiedMs != entry.fingerprintModifiedMs)
            continue;

        result.insert (result.end(), entry.plugins.begin(), entry.plugins.end());
    }

    return result;
}

std::vector<PluginCache::Entry> PluginCache::getBlockedEntries() const
{
    std::vector<Entry> result;

    for (const auto& entry : entries)
        if (entry.isBlocked())
            result.push_back (entry);

    return result;
}

juce::String PluginCache::outcomeToString (Outcome outcome)
{
    switch (outcome)
    {
        case Outcome::success:  return "success";
        case Outcome::crashed:  return "crashed";
        case Outcome::timedOut: return "timedOut";
        case Outcome::failed:   return "failed";
    }

    return "failed";
}

PluginCache::Outcome PluginCache::outcomeFromString (const juce::String& text)
{
    if (text == "success")
        return Outcome::success;

    if (text == "crashed")
        return Outcome::crashed;

    if (text == "timedOut")
        return Outcome::timedOut;

    return Outcome::failed;
}

void PluginCache::clear()
{
    entries.clear();
}

bool PluginCache::load()
{
    entries.clear();

    if (cacheFile == juce::File() || ! cacheFile.existsAsFile())
        return false;

    const auto xml = juce::XmlDocument::parse (cacheFile);

    if (xml == nullptr || ! xml->hasTagName (rootTag))
        return false;

    if (xml->getIntAttribute ("version", 0) != formatVersion)
        return false;

    for (auto* fileElement : xml->getChildIterator())
    {
        if (fileElement == nullptr || ! fileElement->hasTagName (entryTag))
            continue;

        const auto path = fileElement->getStringAttribute ("path");

        if (path.isEmpty())
            continue;

        Entry entry;
        entry.file = juce::File (path);
        entry.fingerprintSize = fileElement->getStringAttribute ("size").getLargeIntValue();
        entry.fingerprintModifiedMs = fileElement->getStringAttribute ("modified").getLargeIntValue();
        entry.outcome = outcomeFromString (fileElement->getStringAttribute ("outcome", "failed"));
        entry.reason = fileElement->getStringAttribute ("reason");
        entry.lastAttemptMs = fileElement->getStringAttribute ("attempt").getLargeIntValue();

        for (auto* pluginElement : fileElement->getChildIterator())
        {
            if (pluginElement == nullptr)
                continue;

            juce::PluginDescription description;

            if (description.loadFromXml (*pluginElement))
                entry.plugins.push_back (std::move (description));
        }

        // A success without any description is not a useful cache hit; treat
        // it as a failure entry so the next scan retries it.
        if (entry.outcome == Outcome::success && entry.plugins.empty())
            entry.outcome = Outcome::failed;

        entries.push_back (std::move (entry));
    }

    return true;
}

bool PluginCache::save() const
{
    if (cacheFile == juce::File())
        return false;

    if (! cacheFile.getParentDirectory().createDirectory())
        return false;

    juce::XmlElement xml (rootTag);
    xml.setAttribute ("version", formatVersion);

    for (const auto& entry : entries)
    {
        auto* fileElement = xml.createNewChildElement (entryTag);
        fileElement->setAttribute ("path", entry.file.getFullPathName());
        fileElement->setAttribute ("size", makeFingerprintAttribute (entry.fingerprintSize));
        fileElement->setAttribute ("modified", makeFingerprintAttribute (entry.fingerprintModifiedMs));
        fileElement->setAttribute ("outcome", outcomeToString (entry.outcome));
        fileElement->setAttribute ("attempt", makeFingerprintAttribute (entry.lastAttemptMs));

        if (entry.reason.isNotEmpty())
            fileElement->setAttribute ("reason", entry.reason);

        for (const auto& description : entry.plugins)
            if (auto pluginElement = description.createXml())
                fileElement->addChildElement (pluginElement.release());
    }

    return xml.writeTo (cacheFile);
}

} // namespace ihub::plugins
