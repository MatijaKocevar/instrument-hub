#include "controller/mapping/MappingFile.h"

#include <juce_core/juce_core.h>

#include <fstream>
#include <functional>
#include <memory>
#include <sstream>

namespace Controller::MappingDetail
{

    namespace
    {
        constexpr juce::int64 maximumArchiveEntryBytes = 4 * 1024 * 1024;
    }

    bool
    readFileContents (const std::filesystem::path& file, std::string& out, std::string& errorText)
    {
        std::ifstream stream (file, std::ios::binary);

        if (!stream)
        {
            errorText = "cannot open '" + file.string () + "'";
            return false;
        }

        std::ostringstream buffer;
        buffer << stream.rdbuf ();

        if (stream.bad ())
        {
            errorText = "cannot read '" + file.string () + "'";
            return false;
        }

        out = buffer.str ();
        return true;
    }

    bool readArchiveEntry (const std::filesystem::path& archive,
                           const std::string& entryName,
                           std::string& out,
                           std::string& errorText)
    {
        const juce::File file (juce::String (archive.string ()));
        juce::ZipFile zip (file);

        if (zip.getNumEntries () <= 0)
        {
            errorText = "cannot read '" + archive.string () + "' as a project container";
            return false;
        }

        const auto* entry = zip.getEntry (juce::String (entryName));

        if (entry == nullptr || entry->uncompressedSize < 0 ||
            entry->uncompressedSize > maximumArchiveEntryBytes)
        {
            errorText = "project container '" + archive.string () + "' has no readable '" +
                        entryName + "' entry";
            return false;
        }

        std::unique_ptr<juce::InputStream> stream (zip.createStreamForEntry (*entry));

        if (stream == nullptr)
        {
            errorText = "cannot open '" + entryName + "' in '" + archive.string () + "'";
            return false;
        }

        juce::MemoryBlock data;
        data.setSize (static_cast<std::size_t> (entry->uncompressedSize));

        if (entry->uncompressedSize > 0 &&
            stream->read (data.getData (), static_cast<int> (entry->uncompressedSize)) !=
                entry->uncompressedSize)
        {
            errorText = "cannot read '" + entryName + "' in '" + archive.string () + "'";
            return false;
        }

        out.clear ();

        if (data.getSize () > 0)
            out.assign (static_cast<const char*> (data.getData ()), data.getSize ());

        return true;
    }

    std::uint64_t hashContents (const std::string& contents)
    {
        return static_cast<std::uint64_t> (std::hash<std::string>{}(contents));
    }

}
