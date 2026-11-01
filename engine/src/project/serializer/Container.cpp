#include "project/serializer/Container.h"

#include <memory>

namespace ihub::project::serializer
{

bool readEntry (juce::ZipFile& zip,
                const juce::String& name,
                juce::int64 maximumBytes,
                juce::MemoryBlock& destination,
                juce::String& error)
{
    const auto* entry = zip.getEntry (name);

    if (entry == nullptr)
    {
        error = "container is missing \"" + name + "\"";
        return false;
    }

    if (entry->uncompressedSize < 0 || entry->uncompressedSize > maximumBytes)
    {
        error = "container entry \"" + name + "\" is too large ("
                + juce::String (entry->uncompressedSize) + " bytes)";
        return false;
    }

    destination.setSize (static_cast<std::size_t> (entry->uncompressedSize));

    if (entry->uncompressedSize == 0)
        return true;

    std::unique_ptr<juce::InputStream> stream (zip.createStreamForEntry (*entry));

    if (stream == nullptr)
    {
        error = "cannot open container entry \"" + name + "\"";
        return false;
    }

    const auto expectedBytes = static_cast<int> (entry->uncompressedSize);
    int totalBytesRead = 0;

    while (totalBytesRead < expectedBytes)
    {
        const auto bytesRead = stream->read (static_cast<char*> (destination.getData()) + totalBytesRead,
                                            expectedBytes - totalBytesRead);

        if (bytesRead <= 0)
            break;

        totalBytesRead += bytesRead;
    }

    if (totalBytesRead != expectedBytes)
    {
        error = "container entry \"" + name + "\" is truncated";
        return false;
    }

    return true;
}

bool addEntry (juce::ZipFile::Builder& builder,
               const juce::MemoryBlock& data,
               const juce::String& name,
               int compressionLevel)
{
    builder.addEntry (std::make_unique<juce::MemoryInputStream> (data, true),
                      compressionLevel,
                      name,
                      juce::Time::getCurrentTime());
    return true;
}

} // namespace ihub::project::serializer
