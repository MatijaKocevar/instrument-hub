#include "SourcePersistence.h"

#include "Session.h"

namespace ihub::app
{

juce::File sourceStateFileFor (const std::filesystem::path& setFile)
{
    return juce::File (juce::String (setFile.string()))
        .getSiblingFile ("set.sources.json");
}

juce::File pluginStateDirectoryFor (const std::filesystem::path& setFile)
{
    return juce::File (juce::String (setFile.string()))
        .getSiblingFile ("set.plugin-state");
}

void applySourceKind (const juce::String& kind, ihub::SessionSlotConfig& slot)
{
    if (kind == "init-tone")
    {
        slot.kind = ihub::SessionSlotKind::initTone;
    }
    else if (kind == "audio-input")
    {
        slot.kind = ihub::SessionSlotKind::audioInput;
    }
    else if (kind == "sampler")
    {
        if (slot.path.isEmpty())
            slot.path = juce::String (slot.samplePath);

        if (slot.path.isNotEmpty() && juce::File (slot.path).exists())
        {
            slot.kind = ihub::SessionSlotKind::sampler;
        }
        else
        {
            slot.kind = ihub::SessionSlotKind::empty;
        }
    }
    else if (kind == "plugin")
    {
        // The set descriptor may carry the bundle path in `path`; older
        // descriptors used `pluginId`, which is only usable when it is a real
        // file path.
        if (slot.path.isEmpty() && juce::File (juce::String (slot.pluginId)).exists())
            slot.path = juce::String (slot.pluginId);

        if (slot.path.isNotEmpty() && juce::File (slot.path).exists())
        {
            slot.kind = ihub::SessionSlotKind::plugin;
        }
        else
        {
            slot.kind = ihub::SessionSlotKind::empty;
        }
    }
    else
    {
        slot.kind = ihub::SessionSlotKind::empty;
    }
}

void loadPersistedSources (ihub::SessionConfig& config,
                           const std::filesystem::path& setFile)
{
    const auto file = sourceStateFileFor (setFile);

    if (! file.existsAsFile())
        return;

    const auto root = juce::JSON::parse (file.loadFileAsString());

    if (! root.isObject())
        return;

    const auto sources = root.getProperty (juce::Identifier ("sources"), {});

    if (! sources.isObject())
        return;

    const auto* object = sources.getDynamicObject();

    if (object == nullptr)
        return;

    for (int index = 0; index < object->getProperties().size(); ++index)
    {
        const auto& name = object->getProperties().getName (index);
        const int slot = name.toString().getIntValue();

        if (slot < 0 || slot >= static_cast<int> (config.slots.size()))
            continue;

        const auto entry = object->getProperties().getValueAt (index);

        if (! entry.isObject())
            continue;

        auto& slotConfig = config.slots[static_cast<std::size_t> (slot)];

        slotConfig.instrumentKind = entry.getProperty (juce::Identifier ("kind"), {})
                                        .toString().toStdString();
        slotConfig.path = entry.getProperty (juce::Identifier ("path"), {}).toString();
        slotConfig.frequency = entry.getProperty (juce::Identifier ("frequency"), 220.0);
        slotConfig.inputChannel = static_cast<int> (
            entry.getProperty (juce::Identifier ("inputChannel"), 0));
        slotConfig.pluginFormat = entry.getProperty (juce::Identifier ("pluginFormat"), "VST3")
                                     .toString().toStdString();
        slotConfig.pluginId = entry.getProperty (juce::Identifier ("pluginId"), {})
                                  .toString().toStdString();
        slotConfig.pluginName = entry.getProperty (juce::Identifier ("pluginName"), {})
                                    .toString().toStdString();
        slotConfig.pluginManufacturer =
            entry.getProperty (juce::Identifier ("pluginManufacturer"), {})
                .toString().toStdString();
        slotConfig.pluginVersion = entry.getProperty (juce::Identifier ("pluginVersion"), {})
                                       .toString().toStdString();
        slotConfig.samplePath = entry.getProperty (juce::Identifier ("samplePath"), {})
                                    .toString().toStdString();

        applySourceKind (juce::String (slotConfig.instrumentKind), slotConfig);
    }
}

void persistSourceAssignment (const std::filesystem::path& setFile,
                              int slot,
                              const ihub::SessionSlotConfig& source)
{
    if (slot < 0 || setFile.empty())
        return;

    const auto file = sourceStateFileFor (setFile);

    auto root = juce::JSON::parse (file.existsAsFile() ? file.loadFileAsString()
                                                       : juce::String());

    if (! root.isObject())
        root = juce::var (new juce::DynamicObject());

    auto* rootObject = root.getDynamicObject();

    if (rootObject == nullptr)
        return;

    auto sources = rootObject->getProperty (juce::Identifier ("sources"));

    if (! sources.isObject())
        sources = juce::var (new juce::DynamicObject());

    auto* sourceObject = sources.getDynamicObject();

    if (sourceObject == nullptr)
        return;

    auto entry = juce::var (new juce::DynamicObject());
    auto* fields = entry.getDynamicObject();

    fields->setProperty (juce::Identifier ("kind"), juce::String (source.instrumentKind));
    fields->setProperty (juce::Identifier ("path"), source.path);
    fields->setProperty (juce::Identifier ("frequency"), source.frequency);
    fields->setProperty (juce::Identifier ("inputChannel"), source.inputChannel);
    fields->setProperty (juce::Identifier ("pluginFormat"), juce::String (source.pluginFormat));
    fields->setProperty (juce::Identifier ("pluginId"), juce::String (source.pluginId));
    fields->setProperty (juce::Identifier ("pluginName"), juce::String (source.pluginName));
    fields->setProperty (juce::Identifier ("pluginManufacturer"), juce::String (source.pluginManufacturer));
    fields->setProperty (juce::Identifier ("pluginVersion"), juce::String (source.pluginVersion));
    fields->setProperty (juce::Identifier ("samplePath"), juce::String (source.samplePath));

    sourceObject->setProperty (juce::Identifier (juce::String (slot)), entry);
    rootObject->setProperty (juce::Identifier ("sources"), sources);

    file.replaceWithText (juce::JSON::toString (root, false));
}

} // namespace ihub::app
