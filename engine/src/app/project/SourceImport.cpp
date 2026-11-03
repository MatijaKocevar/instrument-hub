#include "SourceImport.h"

#include "ProjectLimits.h"

#include "session/PluginStateFile.h"

#include <string>
#include <utility>

namespace ihub::app
{

namespace
{

void importPluginSource (const juce::var& entry,
                         const juce::String& path,
                         project::SlotConfig& slot)
{
    slot.instrument.pluginPath = path.toStdString();
    slot.instrument.pluginFormat =
        entry.getProperty (juce::Identifier ("pluginFormat"), "VST3")
            .toString().toStdString();
    slot.instrument.pluginId =
        entry.getProperty (juce::Identifier ("pluginId"), {}).toString().toStdString();
    slot.instrument.pluginName =
        entry.getProperty (juce::Identifier ("pluginName"), {}).toString().toStdString();
    slot.instrument.pluginManufacturer =
        entry.getProperty (juce::Identifier ("pluginManufacturer"), {})
            .toString().toStdString();
    slot.instrument.pluginVersion =
        entry.getProperty (juce::Identifier ("pluginVersion"), {})
            .toString().toStdString();

    if (slot.name.empty())
        slot.name = slot.instrument.pluginName;
}

}

bool importSourceAssignments (const juce::File& sourcesFile,
                              std::map<int, project::SlotConfig>& slots,
                              juce::String& error)
{
    if (! sourcesFile.existsAsFile())
        return true;

    const auto root = juce::JSON::parse (sourcesFile.loadFileAsString());

    if (! root.isObject())
    {
        error = "sources file is not a JSON object: " + sourcesFile.getFullPathName();
        return false;
    }

    const auto sources = root.getProperty (juce::Identifier ("sources"), {});

    if (! sources.isObject())
        return true;

    const auto* object = sources.getDynamicObject();

    if (object == nullptr)
        return true;

    for (int index = 0; index < object->getProperties().size(); ++index)
    {
        const auto& name = object->getProperties().getName (index);
        const int slotIndex = name.toString().getIntValue();

        if (slotIndex < 0 || slotIndex >= maximumProjectSlots)
            continue;

        const auto entry = object->getProperties().getValueAt (index);

        if (! entry.isObject())
            continue;

        auto& slot = slots[slotIndex];
        slot.index = slotIndex;

        const auto kind = entry.getProperty (juce::Identifier ("kind"), {}).toString();
        const auto path = entry.getProperty (juce::Identifier ("path"), {}).toString();

        if (! project::sourceKindFromString (kind.toStdString(), slot.instrument.kind))
            slot.instrument.kind = project::SourceKind::empty;

        if (slot.instrument.kind == project::SourceKind::plugin)
        {
            importPluginSource (entry, path, slot);
        }
        else if (slot.instrument.kind == project::SourceKind::sampler)
        {
            const auto samplePath =
                entry.getProperty (juce::Identifier ("samplePath"), {}).toString();

            slot.instrument.samplePath =
                (samplePath.isNotEmpty() ? samplePath : path).toStdString();
        }
        else if (slot.instrument.kind == project::SourceKind::audioInput)
        {
            slot.instrument.inputChannel =
                static_cast<int> (entry.getProperty (juce::Identifier ("inputChannel"), 0));
        }
    }

    return true;
}

bool importPluginStates (const juce::File& pluginStateDirectory,
                         std::map<int, project::SlotConfig>& slots,
                         std::vector<project::PluginStateChunk>& chunks,
                         juce::String& error)
{
    if (! pluginStateDirectory.isDirectory())
        return true;

    for (int slotIndex = 0; slotIndex < maximumProjectSlots; ++slotIndex)
    {
        const auto file = pluginStateDirectory
                              .getChildFile ("slot-" + juce::String (slotIndex) + ".bin");

        if (! file.existsAsFile())
            continue;

        session::PluginStateFileData stored;

        if (! session::readPluginStateFileData (file, slotIndex, stored))
        {
            error = "cannot read plugin state " + file.getFullPathName();
            return false;
        }

        project::PluginStateChunk chunk;
        chunk.uuid = project::makePluginStateId();
        chunk.format = "VST3";
        chunk.pluginId = stored.pluginId.toStdString();
        chunk.name = stored.pluginName.toStdString();
        chunk.data = stored.data;

        auto& slot = slots[slotIndex];
        slot.index = slotIndex;
        slot.instrument.kind = project::SourceKind::plugin;
        slot.instrument.pluginPath = stored.pluginPath.toStdString();
        slot.instrument.pluginId = stored.pluginId.toStdString();
        slot.instrument.pluginName = stored.pluginName.toStdString();
        slot.instrument.pluginState = chunk.uuid;

        if (slot.instrument.pluginFormat.empty())
            slot.instrument.pluginFormat = "VST3";

        if (slot.name.empty())
            slot.name = chunk.name;

        chunks.push_back (std::move (chunk));
    }

    return true;
}

}
