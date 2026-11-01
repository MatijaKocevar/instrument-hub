#include "PluginStateFile.h"

namespace ihub::session
{

// One `slot-<n>.bin` per patch lives next to `set.yaml`. The file is a small
// header, a JSON metadata block and the opaque plugin blob, so the plugin
// chunk itself is never inlined in a JSON document (plan/DECISIONS.md).
// `replaceWithData()` writes a sibling temporary file and renames it, so an
// interrupted save cannot truncate a good patch state.

constexpr juce::uint32 pluginStateFileMagic = 0x49485053u; // "IHPS"
constexpr int pluginStateFileVersion = 1;
constexpr juce::int64 maximumPluginStateFileBytes = 64 * 1024 * 1024;
constexpr int maximumPluginStateMetadataBytes = 64 * 1024;

juce::File pluginStateFileFor (const juce::File& directory, int slot)
{
    return directory.getChildFile ("slot-" + juce::String (slot) + ".bin");
}

bool writePluginStateFile (const juce::File& file,
                           int slot,
                           const juce::MemoryBlock& data,
                           const juce::String& pluginPath,
                           const juce::String& pluginId,
                           const juce::String& pluginName,
                           std::uint64_t revision)
{
    if (data.getSize() == 0
        || static_cast<juce::int64> (data.getSize()) > maximumPluginStateFileBytes)
        return false;

    auto metadata = juce::var (new juce::DynamicObject());
    auto* fields = metadata.getDynamicObject();
    fields->setProperty ("slot", slot);
    fields->setProperty ("revision", static_cast<juce::int64> (revision));
    fields->setProperty ("path", pluginPath);
    fields->setProperty ("pluginId", pluginId);
    fields->setProperty ("pluginName", pluginName);

    const auto json = juce::JSON::toString (metadata, false);
    const auto metadataSize = json.getNumBytesAsUTF8();

    if (metadataSize <= 0 || metadataSize > maximumPluginStateMetadataBytes)
        return false;

    juce::MemoryOutputStream stream;
    stream.writeInt (static_cast<int> (pluginStateFileMagic));
    stream.writeInt (pluginStateFileVersion);
    stream.writeInt (static_cast<int> (metadataSize));
    stream.write (json.toRawUTF8(), metadataSize);
    stream.writeInt64 (static_cast<juce::int64> (data.getSize()));

    if (data.getSize() > 0)
        stream.write (data.getData(), data.getSize());

    if (! file.getParentDirectory().createDirectory().wasOk())
        return false;

    return file.replaceWithData (stream.getData(), stream.getDataSize());
}

bool readPluginStateFileData (const juce::File& file, int expectedSlot,
                              PluginStateFileData& result)
{
    if (! file.existsAsFile()
        || file.getSize() <= 0
        || file.getSize() > maximumPluginStateFileBytes)
        return false;

    juce::FileInputStream stream (file);

    if (! stream.openedOk())
        return false;

    if (stream.readInt() != static_cast<int> (pluginStateFileMagic)
        || stream.readInt() != pluginStateFileVersion)
        return false;

    const int metadataSize = stream.readInt();

    if (metadataSize <= 0 || metadataSize > maximumPluginStateMetadataBytes)
        return false;

    juce::MemoryBlock metadataBlock;
    metadataBlock.setSize (static_cast<std::size_t> (metadataSize));

    if (stream.read (metadataBlock.getData(), metadataSize) != metadataSize)
        return false;

    const auto metadata = juce::JSON::parse (
        juce::String::fromUTF8 (static_cast<const char*> (metadataBlock.getData()),
                                metadataSize));

    if (! metadata.isObject())
        return false;

    const auto* fields = metadata.getDynamicObject();

    if (fields == nullptr
        || static_cast<int> (fields->getProperty ("slot")) != expectedSlot)
        return false;

    const juce::int64 dataSize = stream.readInt64();

    if (dataSize <= 0 || dataSize > maximumPluginStateFileBytes)
        return false;

    const auto remaining = file.getSize() - stream.getPosition();

    if (remaining < dataSize)
        return false;

    result.data.setSize (static_cast<std::size_t> (dataSize));

    if (stream.read (result.data.getData(), static_cast<int> (dataSize))
        != static_cast<int> (dataSize))
        return false;

    result.pluginPath = fields->getProperty ("path").toString();
    result.pluginId = fields->getProperty ("pluginId").toString();
    result.pluginName = fields->getProperty ("pluginName").toString();
    result.revision = static_cast<std::uint64_t> (
        static_cast<juce::int64> (fields->getProperty ("revision")));

    return true;
}

/** True when a stored blob was written for the plugin now assigned to the
    slot. An empty metadata field is accepted (older sidecar or a descriptor
    without a bundle path), but a conflicting value is not. */
bool pluginStateMatches (const PluginStateFileData& stored,
                         const juce::String& pluginPath,
                         const juce::String& pluginId)
{
    if (stored.pluginPath.isNotEmpty() && pluginPath.isNotEmpty()
        && stored.pluginPath != pluginPath)
        return false;

    if (stored.pluginId.isNotEmpty() && pluginId.isNotEmpty()
        && stored.pluginId != pluginId)
        return false;

    return true;
}

} // namespace ihub::session
