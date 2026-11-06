#include "ProjectImport.h"

#include "MappingImport.h"
#include "SourceImport.h"

#include "BundledTemplates.h"
#include "controller/Mapping.h"
#include "controller/MappingYaml.h"
#include "project/Serializer.h"

#include <map>
#include <string>
#include <utility>

namespace ihub::app
{

namespace
{

bool importBundledSetText (const juce::String& text,
                           const juce::String& sourceName,
                           const juce::File& pluginStateDirectory,
                           const juce::File& sourcesFile,
                           project::SetFile& destination,
                           juce::String& error)
{
    const auto utf8 = text.toStdString();

    Controller::SetMapping document;
    Controller::MappingError mappingError;

    if (! Controller::parseSetMappingYaml (utf8, document, mappingError))
    {
        error = sourceName + ":" + juce::String (mappingError.line) + ": "
                + juce::String (mappingError.message);
        return false;
    }

    project::SetFile set;
    set.name = document.name.empty() ? "Untitled Set" : document.name;
    set.mappingYaml = utf8;

    std::map<int, project::SlotConfig> slots;
    importMappingPatches (document, slots);

    if (! importSourceAssignments (sourcesFile, slots, error))
        return false;

    std::vector<project::PluginStateChunk> chunks;

    if (! importPluginStates (pluginStateDirectory, slots, chunks, error))
        return false;

    set.slots.reserve (slots.size());

    for (auto& [slotIndex, slot] : slots)
    {
        slot.index = slotIndex;
        set.slots.push_back (std::move (slot));
    }

    set.pluginStates = std::move (chunks);

    if (! set.validate (error))
        return false;

    destination = std::move (set);
    return true;
}

juce::String embeddedTemplate (const char* data, int size)
{
    if (data == nullptr || size <= 0)
        return {};

    return juce::String::fromUTF8 (data, size);
}

}

bool importBundledSet (const juce::File& mappingFile,
                       const juce::File& pluginStateDirectory,
                       const juce::File& sourcesFile,
                       project::SetFile& destination,
                       juce::String& error)
{
    if (! mappingFile.existsAsFile())
    {
        error = "bundled mapping does not exist: " + mappingFile.getFullPathName();
        return false;
    }

    return importBundledSetText (mappingFile.loadFileAsString(),
                                 "bundled mapping " + mappingFile.getFullPathName(),
                                 pluginStateDirectory,
                                 sourcesFile,
                                 destination,
                                 error);
}

bool ensureProjectFile (const juce::File& projectFile,
                        juce::String& error,
                        bool* created)
{
    if (created != nullptr)
        *created = false;

    if (projectFile.existsAsFile())
        return true;

    if (! writeBundledProject (projectFile, {}, error))
        return false;

    if (created != nullptr)
        *created = true;

    return true;
}

bool writeBundledProject (const juce::File& projectFile,
                          const juce::String& name,
                          juce::String& error)
{
    const auto bundled = Controller::Mapping::findDefaultSetFile();
    const juce::File bundledFile (bundled.empty() ? juce::String()
                                                  : juce::String (bundled.string()));

    project::SetFile set;
    bool imported = false;

    if (bundledFile.existsAsFile())
    {
        auto mappingFile = bundledFile;
        const auto trackFile = mappingFile.getSiblingFile ("tracks-set.yaml");
        const auto blankFile = mappingFile.getSiblingFile ("blank-set.yaml");
        const bool tracks = trackFile.existsAsFile();
        const bool blank = ! tracks && blankFile.existsAsFile();

        if (tracks)
            mappingFile = trackFile;
        else if (blank)
            mappingFile = blankFile;

        imported = importBundledSet (
            mappingFile,
            tracks || blank ? juce::File()
                            : mappingFile.getSiblingFile ("set.plugin-state"),
            tracks || blank ? juce::File()
                            : mappingFile.getSiblingFile ("set.sources.json"),
            set,
            error);
    }
    else if (const auto tracks = embeddedTemplate (BundledData::tracksset_yaml,
                                                    BundledData::tracksset_yamlSize);
             tracks.isNotEmpty())
    {
        imported = importBundledSetText (tracks, "bundled tracks template",
                                         {}, {}, set, error);
    }
    else if (const auto blank = embeddedTemplate (BundledData::blankset_yaml,
                                                   BundledData::blankset_yamlSize);
             blank.isNotEmpty())
    {
        imported = importBundledSetText (blank, "bundled blank template",
                                         {}, {}, set, error);
    }
    else
    {
        error = "no bundled project template is available";
        return false;
    }

    if (! imported)
        return false;

    if (name.isNotEmpty())
        set.name = name.toStdString();

    return project::Serializer::save (set, projectFile, error);
}

}
