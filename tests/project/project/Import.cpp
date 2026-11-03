#include "Import.h"

#include "Support.h"

#include "app/ProjectSetup.h"
#include "app/project/ProjectImport.h"
#include "project/Serializer.h"
#include "project/SetFile.h"
#include "session/PluginStateFile.h"

#include <memory>
#include <string>

namespace ProjectTest
{

namespace
{

int pluginStateFileCount (const juce::File& directory)
{
    int count = 0;

    for (int slot = 0; slot < 64; ++slot)
        if (directory.getChildFile ("slot-" + juce::String (slot) + ".bin").existsAsFile())
            ++count;

    return count;
}

}

void runImportChecks()
{
    const auto directory = makeTemporaryDirectory();
    const auto projectFile = directory.getChildFile ("Imported.ihub");

    const auto resolved = ihub::app::resolveProjectFile (
        "--project " + projectFile.getFullPathName());

    check (resolved == projectFile,
           "resolveProjectFile() honours --project");

    check (ihub::app::resolveProjectFile ("")
               == ihub::app::defaultProjectFile(),
           "resolveProjectFile() falls back to the default project");

    juce::String error;
    bool created = false;

    check (ihub::app::ensureProjectFile (resolved, error, &created),
           "the missing project imports the blank template: " + error.toStdString());
    check (created, "the first launch reports that it created the project");
    check (projectFile.existsAsFile(), "the project file exists after the import");

    ihub::project::SetFile set;

    check (ihub::project::Serializer::load (projectFile, set, error),
           "the imported project loads: " + error.toStdString());

    const auto tracksFile = bundledTracksSetFile();
    check (tracksFile.existsAsFile(), "the track mapping template exists");

    check (set.slots.size() == 8,
           "the blank project carries eight tracks (got "
               + std::to_string (set.slots.size()) + ")");
    check (! set.mappingYaml.empty(), "the project keeps the raw mapping YAML");
    check (set.mappingYaml == tracksFile.loadFileAsString().toStdString(),
           "mapping.yaml is the track template byte for byte");
    check (set.pluginStates.empty(), "a blank project carries no plugin state");

    for (int index = 0; index < 8; ++index)
    {
        check (index < static_cast<int> (set.slots.size())
                   && set.slots[static_cast<std::size_t> (index)].index == index,
               "the blank project keeps track slot " + std::to_string (index));
    }

    if (! set.slots.empty())
    {
        check (set.slots[0].instrument.kind == ihub::project::SourceKind::empty,
               "slot 0 starts with no source");
        check (set.slots[0].instrument.name.empty(),
               "slot 0 starts without a patch name");
        check (set.slots[0].name == "Track 1",
               "slot 0 starts with its track name");
        check (set.slots[0].fxChain.empty(),
               "slot 0 starts without FX");
    }

    ihub::project::SetFile bundled;

    check (ihub::app::importBundledSet (bundledMappingFile(),
                                        bundledPluginStateDirectory(),
                                        bundledSourcesFile(),
                                        bundled,
                                        error),
           "the starter set still imports through importBundledSet: "
               + error.toStdString());

    const int sidecarStates = pluginStateFileCount (bundledPluginStateDirectory());
    check (sidecarStates > 0,
           "the bundled set has plugin-state sidecars (got "
               + std::to_string (sidecarStates) + ")");
    check (static_cast<int> (bundled.pluginStates.size()) == sidecarStates,
           "every sidecar blob becomes one project chunk");

    const int slots[] { 0, 48, 56 };

    for (const int slotIndex : slots)
    {
        const auto file = bundledPluginStateDirectory()
                              .getChildFile ("slot-" + juce::String (slotIndex) + ".bin");

        if (! file.existsAsFile()
            || slotIndex >= static_cast<int> (bundled.slots.size()))
            continue;

        ihub::session::PluginStateFileData stored;
        check (ihub::session::readPluginStateFileData (file, slotIndex, stored),
               "sidecar slot " + std::to_string (slotIndex) + " is readable");

        const auto& slot = bundled.slots[static_cast<std::size_t> (slotIndex)];
        check (slot.instrument.kind == ihub::project::SourceKind::plugin,
               "slot " + std::to_string (slotIndex) + " imports as a plugin");
        check (slot.instrument.pluginPath == stored.pluginPath.toStdString(),
               "slot " + std::to_string (slotIndex) + " keeps its plugin path");
        check (slot.instrument.pluginId == stored.pluginId.toStdString(),
               "slot " + std::to_string (slotIndex) + " keeps its plugin id");
        check (! slot.instrument.pluginState.empty(),
               "slot " + std::to_string (slotIndex) + " owns a state reference");

        const auto* chunk = bundled.findPluginState (slot.instrument.pluginState);

        check (chunk != nullptr
                   && chunk->data.getSize() == stored.data.getSize(),
               "slot " + std::to_string (slotIndex)
                   + " keeps the plugin-state bytes");
    }

    juce::String secondError;
    bool secondCreated = true;

    check (ihub::app::ensureProjectFile (projectFile, secondError, &secondCreated),
           "an existing project opens without importing again");
    check (! secondCreated, "the second launch does not re-import");

    check (projectFile.deleteFile(), "the project file is deleted");
    check (ihub::app::ensureProjectFile (projectFile, error, &created),
           "deleting the project makes the next launch import it again: "
               + error.toStdString());
    check (created, "the re-import is reported once");

    ihub::project::SetFile reimported;
    check (ihub::project::Serializer::load (projectFile, reimported, error),
           "the re-imported project loads: " + error.toStdString());
    check (reimported.slots.size() == set.slots.size(),
           "the re-import has the same patch count");
    check (reimported.pluginStates.size() == set.pluginStates.size(),
           "the re-import has the same plugin-state count");

    if (reimported.slots.empty() || set.slots.empty())
    {
        directory.deleteRecursively();
        return;
    }

    check (reimported.slots[0].instrument.pluginPath
               == set.slots[0].instrument.pluginPath,
           "the re-import keeps slot 0's plugin path");

    directory.deleteRecursively();
}

}
