#include "Serializer.h"

#include "project/serializer/Container.h"
#include "project/serializer/PluginStateBlob.h"
#include "project/serializer/SetManifest.h"

#include <utility>

namespace ihub::project
{

    bool Serializer::save (const SetFile& set, const juce::File& file, juce::String& error)
    {
        if (!set.validate (error))
            return false;

        const auto manifest = serializer::makeManifest (set);

        juce::ZipFile::Builder builder;

        const juce::MemoryBlock manifestData (
            manifest.toRawUTF8 (), static_cast<std::size_t> (manifest.getNumBytesAsUTF8 ()));
        serializer::addEntry (builder, manifestData, manifestEntry, 6);

        const juce::MemoryBlock mappingData (set.mappingYaml.data (), set.mappingYaml.size ());
        serializer::addEntry (builder, mappingData, mappingEntry, 6);

        for (const auto& chunk : set.pluginStates)
        {
            serializer::addEntry (
                builder, chunk.data, juce::String (pluginStateEntryPath (chunk.uuid)), 6);
        }

        const auto parent = file.getParentDirectory ();

        if (!parent.exists () && !parent.createDirectory ())
        {
            error = "cannot create directory " + parent.getFullPathName ();
            return false;
        }

        const auto temporary = parent.getChildFile (file.getFileName () + ".tmp");
        temporary.deleteFile ();

        {
            juce::FileOutputStream output (temporary);

            if (!output.openedOk ())
            {
                error = "cannot open " + temporary.getFullPathName () + " for writing";
                temporary.deleteFile ();
                return false;
            }

            if (!builder.writeToStream (output, nullptr))
            {
                error = "could not write the .ihub archive";
                temporary.deleteFile ();
                return false;
            }

            output.flush ();

            if (!output.getStatus ().wasOk ())
            {
                error =
                    "could not flush the .ihub archive: " + output.getStatus ().getErrorMessage ();
                temporary.deleteFile ();
                return false;
            }
        }

        if (file.existsAsFile ())
        {
            const auto backup = parent.getChildFile (file.getFileName () + ".bak");
            backup.deleteFile ();

            if (!file.copyFileTo (backup))
            {
                error = "could not keep the previous .ihub as " + backup.getFullPathName ();
                temporary.deleteFile ();
                return false;
            }
        }

        if (!temporary.replaceFileIn (file))
        {
            error = "could not replace " + file.getFullPathName ();
            temporary.deleteFile ();
            return false;
        }

        error.clear ();
        return true;
    }

    bool Serializer::load (const juce::File& file, SetFile& destination, juce::String& error)
    {
        if (!file.existsAsFile ())
        {
            error = "set file does not exist: " + file.getFullPathName ();
            return false;
        }

        juce::ZipFile zip (file);

        if (zip.getNumEntries () <= 0)
        {
            error = "not a readable .ihub container: " + file.getFullPathName ();
            return false;
        }

        juce::int64 totalUncompressed = 0;

        for (int i = 0; i < zip.getNumEntries (); ++i)
        {
            const auto* entry = zip.getEntry (i);

            if (entry == nullptr)
            {
                error = "corrupt .ihub container: missing entry metadata";
                return false;
            }

            if (entry->uncompressedSize < 0)
            {
                error = "corrupt .ihub container: negative entry size";
                return false;
            }

            totalUncompressed += entry->uncompressedSize;

            if (totalUncompressed > maximumTotalUncompressedBytes)
            {
                error = "container expands to more than the supported maximum";
                return false;
            }
        }

        juce::MemoryBlock manifest;

        if (!serializer::readEntry (zip, manifestEntry, maximumManifestBytes, manifest, error))
            return false;

        SetFile set;
        juce::int64 declaredMappingBytes = -1;

        if (!serializer::parseManifest (manifest, set, declaredMappingBytes, error))
            return false;

        if (!serializer::parsePluginStates (manifest, zip, set, error))
            return false;

        juce::MemoryBlock mappingData;

        if (!serializer::readEntry (zip, mappingEntry, maximumMappingBytes, mappingData, error))
            return false;

        if (declaredMappingBytes >= 0 &&
            declaredMappingBytes != static_cast<juce::int64> (mappingData.getSize ()))
        {
            error = "set.mapping.bytes does not match mapping.yaml (" +
                    juce::String (declaredMappingBytes) + " declared, " +
                    juce::String (static_cast<juce::int64> (mappingData.getSize ())) + " stored)";
            return false;
        }

        if (mappingData.getSize () > 0)
            set.mappingYaml.assign (static_cast<const char*> (mappingData.getData ()),
                                    mappingData.getSize ());

        if (!set.validate (error))
            return false;

        destination = std::move (set);
        error.clear ();
        return true;
    }

}
