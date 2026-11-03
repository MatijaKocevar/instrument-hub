#include "ProjectImport.h"

#include "MappingImport.h"
#include "SourceImport.h"

#include "controller/Mapping.h"
#include "controller/MappingYaml.h"
#include "project/Serializer.h"

#include <map>
#include <string>
#include <utility>

namespace ihub::app
{

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

    const auto text = mappingFile.loadFileAsString().toStdString();

    Controller::SetMapping document;
    Controller::MappingError mappingError;

    if (! Controller::parseSetMappingYaml (text, document, mappingError))
    {
        error = "bundled mapping " + mappingFile.getFullPathName() + ":"
                + juce::String (mappingError.line) + ": "
                + juce::String (mappingError.message);
        return false;
    }

    project::SetFile set;
    set.name = document.name.empty() ? "Untitled Set" : document.name;
    set.mappingYaml = text;

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

    if (bundled.empty())
    {
        error = "no bundled set.yaml is available to import";
        return false;
    }

    juce::File mappingFile (juce::String (bundled.string()));
    const auto trackFile = mappingFile.getSiblingFile ("tracks-set.yaml");
    const auto blankFile = mappingFile.getSiblingFile ("blank-set.yaml");
    const bool tracks = trackFile.existsAsFile();
    const bool blank = ! tracks && blankFile.existsAsFile();

    if (tracks)
        mappingFile = trackFile;
    else if (blank)
        mappingFile = blankFile;

    project::SetFile set;

    if (! importBundledSet (mappingFile,
                            tracks || blank
                                ? juce::File()
                                : mappingFile.getSiblingFile ("set.plugin-state"),
                            tracks || blank
                                ? juce::File()
                                : mappingFile.getSiblingFile ("set.sources.json"),
                            set,
                            error))
        return false;

    if (name.isNotEmpty())
        set.name = name.toStdString();

    return project::Serializer::save (set, projectFile, error);
}

}
