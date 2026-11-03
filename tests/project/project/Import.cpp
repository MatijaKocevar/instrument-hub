#include "Import.h"

#include "Support.h"

#include "app/ProjectSetup.h"
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
           "the missing project imports from the bundled set: " + error.toStdString());
    check (created, "the first launch reports that it imported the project");
    check (projectFile.existsAsFile(), "the project file exists after the import");

    ihub::project::SetFile set;

    check (ihub::project::Serializer::load (projectFile, set, error),
           "the imported project loads: " + error.toStdString());

    if (set.slots.empty())
    {
        directory.deleteRecursively();
        return;
    }

    check (set.slots.size() == 64,
           "the project carries 64 patches (got "
               + std::to_string (set.slots.size()) + ")");
    check (! set.mappingYaml.empty(), "the project keeps the raw mapping YAML");

    const auto mappingText = bundledMappingFile().loadFileAsString().toStdString();
    check (set.mappingYaml == mappingText, "mapping.yaml is stored byte for byte");

    const int sidecarStates = pluginStateFileCount (bundledPluginStateDirectory());
    check (sidecarStates > 0,
           "the bundled set has plugin-state sidecars (got "
               + std::to_string (sidecarStates) + ")");
    check (static_cast<int> (set.pluginStates.size()) == sidecarStates,
           "every sidecar blob becomes one project chunk");

    const int slots[] { 0, 48, 56 };

    for (const int slotIndex : slots)
    {
        const auto file = bundledPluginStateDirectory()
                              .getChildFile ("slot-" + juce::String (slotIndex) + ".bin");

        if (! file.existsAsFile()
            || slotIndex >= static_cast<int> (set.slots.size()))
            continue;

        ihub::session::PluginStateFileData stored;
        check (ihub::session::readPluginStateFileData (file, slotIndex, stored),
               "sidecar slot " + std::to_string (slotIndex) + " is readable");

        const auto& slot = set.slots[static_cast<std::size_t> (slotIndex)];
        check (slot.instrument.kind == ihub::project::SourceKind::plugin,
               "slot " + std::to_string (slotIndex) + " imports as a plugin");
        check (slot.instrument.pluginPath == stored.pluginPath.toStdString(),
               "slot " + std::to_string (slotIndex) + " keeps its plugin path");
        check (slot.instrument.pluginId == stored.pluginId.toStdString(),
               "slot " + std::to_string (slotIndex) + " keeps its plugin id");
        check (! slot.instrument.pluginState.empty(),
               "slot " + std::to_string (slotIndex) + " owns a state reference");

        const auto* chunk = set.findPluginState (slot.instrument.pluginState);

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

    if (! set.slots[0].instrument.pluginState.empty())
    {
        const auto* original = set.findPluginState (set.slots[0].instrument.pluginState);
        const auto* imported = reimported.findPluginState (
            reimported.slots[0].instrument.pluginState);

        check (original != nullptr && imported != nullptr
                   && original->data == imported->data,
               "the re-import keeps slot 0's plugin-state bytes");
    }

    directory.deleteRecursively();
}

}
