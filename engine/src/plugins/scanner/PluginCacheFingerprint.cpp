#include "../PluginCache.h"

namespace ihub::plugins
{
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

}
