#include "PluginStateStore.h"

#include "plugins/PluginInstrument.h"

#include <juce_core/juce_core.h>

#include <utility>

namespace ihub::session
{

namespace
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

struct PluginStateFile
{
    juce::MemoryBlock data;
    juce::String pluginPath;
    juce::String pluginId;
    juce::String pluginName;
    std::uint64_t revision = 0;
};

bool readPluginStateFileData (const juce::File& file, int expectedSlot, PluginStateFile& result)
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
bool pluginStateMatches (const PluginStateFile& stored,
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

} // namespace

PluginStateStore::PluginStateStore (int slotCapacity)
    : states (static_cast<std::size_t> (juce::jmax (0, slotCapacity))),
      instruments (static_cast<std::size_t> (juce::jmax (0, slotCapacity)), nullptr)
{
}

void PluginStateStore::setDirectory (const juce::File& newDirectory)
{
    {
        const std::lock_guard<std::mutex> lock (mutex);
        directory = newDirectory;
    }

    (void) loadAll();
}

juce::File PluginStateStore::getDirectory() const
{
    const std::lock_guard<std::mutex> lock (mutex);
    return directory;
}

bool PluginStateStore::loadAll()
{
    const std::lock_guard<std::mutex> lock (mutex);

    for (auto& state : states)
        state = {};

    if (directory == juce::File())
        return false;

    if (! directory.isDirectory())
        return true; // nothing captured yet

    bool allRead = true;

    for (int slot = 0; slot < static_cast<int> (states.size()); ++slot)
    {
        const auto file = pluginStateFileFor (directory, slot);

        if (! file.existsAsFile())
            continue;

        if (! readFileLocked (slot, file))
        {
            allRead = false;
            juce::Logger::writeToLog ("session: unreadable plugin state "
                                      + file.getFullPathName());
        }
    }

    return allRead;
}

bool PluginStateStore::saveAll()
{
    juce::File currentDirectory;

    {
        const std::lock_guard<std::mutex> lock (mutex);
        currentDirectory = directory;
    }

    if (currentDirectory == juce::File())
        return false;

    bool saved = true;

    for (int slot = 0; slot < static_cast<int> (states.size()); ++slot)
    {
        bool hasData = false;

        {
            const std::lock_guard<std::mutex> lock (mutex);
            hasData = states[static_cast<std::size_t> (slot)].data.getSize() > 0;
        }

        if (hasData && ! write (slot))
            saved = false;
    }

    return saved;
}

bool PluginStateStore::capture (int slot, const PluginStateIdentity& identity)
{
    if (slot < 0 || slot >= static_cast<int> (states.size()))
        return false;

    bool changed = false;
    bool captured = false;
    juce::File currentDirectory;

    {
        const std::lock_guard<std::mutex> lock (mutex);

        currentDirectory = directory;

        if (slot >= static_cast<int> (instruments.size())
            || instruments[static_cast<std::size_t> (slot)] == nullptr)
            return false;

        auto& state = states[static_cast<std::size_t> (slot)];
        auto data = instruments[static_cast<std::size_t> (slot)]->getStateInformation();

        if (data.getSize() > 0)
        {
            captured = true;

            if (state.data != data)
            {
                state.data = std::move (data);
                ++state.revision;
                changed = true;
            }

            state.pluginPath = identity.pluginPath;
            state.pluginId = identity.pluginId;
            state.pluginName = identity.pluginName;

            // The resident instance now holds this revision.
            state.appliedRevision = state.revision;
        }
    }

    if (! captured)
        return false;

    if (changed && currentDirectory != juce::File())
        return write (slot);

    return true;
}

void PluginStateStore::apply (int slot,
                              plugins::PluginInstrument& instrument,
                              const PluginStateIdentity& identity,
                              bool force)
{
    juce::MemoryBlock data;
    std::uint64_t revision = 0;

    {
        const std::lock_guard<std::mutex> lock (mutex);

        if (slot < 0 || slot >= static_cast<int> (states.size()))
            return;

        auto& state = states[static_cast<std::size_t> (slot)];

        if (state.data.getSize() == 0)
            return;

        // A blob belongs to the plugin it was captured from. Never hand an old
        // patch's state to a different plug-in assigned to the same slot.
        PluginStateFile stored;
        stored.pluginPath = state.pluginPath;
        stored.pluginId = state.pluginId;

        if (! pluginStateMatches (stored, identity.pluginPath, identity.pluginId))
        {
            state.appliedRevision = state.revision;
            return;
        }

        // `appliedRevision` describes the resident instance. A fresh instance
        // from the warm-pool factory must be restored even when the slot
        // revision has not moved: after an eviction the old instance is gone,
        // so its applied marker says nothing about the replacement.
        if (! force && state.appliedRevision == state.revision
            && state.appliedRevision != 0)
            return;

        data = state.data;
        revision = state.revision;
    }

    // VST3 state restoration takes the message-manager lock on some formats,
    // so never hold the state mutex across it: the message thread may be
    // waiting for that same mutex (for example in getInstrument()) and could
    // not pump the lock the plugin needs.
    const bool accepted = instrument.setStateInformation (data);

    if (! accepted)
    {
        juce::Logger::writeToLog ("session: slot " + juce::String (slot)
                                  + " rejected stored plugin state \""
                                  + (identity.pluginName.isEmpty()
                                         ? juce::String ("plugin")
                                         : identity.pluginName)
                                  + "\"");
    }

    // Mark it applied even after a rejection: retrying a blob the plugin
    // refuses on every warm-up would only delay arming. If a capture changed
    // the revision while the plugin was restoring, leave the newer blob for
    // the next arm instead of claiming it was applied.
    const std::lock_guard<std::mutex> lock (mutex);

    if (slot >= 0 && slot < static_cast<int> (states.size())
        && states[static_cast<std::size_t> (slot)].revision == revision)
        states[static_cast<std::size_t> (slot)].appliedRevision = revision;
}

void PluginStateStore::applyToRegistered (int slot, const PluginStateIdentity& identity)
{
    plugins::PluginInstrument* instrument = getInstrument (slot);

    if (instrument != nullptr)
        apply (slot, *instrument, identity);
}

void PluginStateStore::clear (int slot)
{
    if (slot < 0 || slot >= static_cast<int> (states.size()))
        return;

    juce::File currentDirectory;

    {
        const std::lock_guard<std::mutex> lock (mutex);

        currentDirectory = directory;
        states[static_cast<std::size_t> (slot)] = {};
    }

    if (currentDirectory != juce::File())
        pluginStateFileFor (currentDirectory, slot).deleteFile();
}

bool PluginStateStore::hasState (int slot) const noexcept
{
    if (slot < 0 || slot >= static_cast<int> (states.size()))
        return false;

    const std::lock_guard<std::mutex> lock (mutex);
    return states[static_cast<std::size_t> (slot)].data.getSize() > 0;
}

juce::int64 PluginStateStore::getStateSize (int slot) const noexcept
{
    if (slot < 0 || slot >= static_cast<int> (states.size()))
        return 0;

    const std::lock_guard<std::mutex> lock (mutex);
    return static_cast<juce::int64> (states[static_cast<std::size_t> (slot)].data.getSize());
}

void PluginStateStore::registerInstrument (int slot,
                                           plugins::PluginInstrument& instrument) noexcept
{
    if (slot < 0 || slot >= static_cast<int> (instruments.size()))
        return;

    const std::lock_guard<std::mutex> lock (mutex);
    instruments[static_cast<std::size_t> (slot)] = &instrument;
}

void PluginStateStore::unregisterInstrument (plugins::PluginInstrument& instrument) noexcept
{
    const std::lock_guard<std::mutex> lock (mutex);

    for (auto& entry : instruments)
        if (entry == &instrument)
            entry = nullptr;
}

plugins::PluginInstrument* PluginStateStore::getInstrument (int slot) const noexcept
{
    if (slot < 0 || slot >= static_cast<int> (instruments.size()))
        return nullptr;

    const std::lock_guard<std::mutex> lock (mutex);
    return instruments[static_cast<std::size_t> (slot)];
}

int PluginStateStore::findSlotForInstrument (plugins::PluginInstrument& instrument) const noexcept
{
    const std::lock_guard<std::mutex> lock (mutex);

    for (int slot = 0; slot < static_cast<int> (instruments.size()); ++slot)
        if (instruments[static_cast<std::size_t> (slot)] == &instrument)
            return slot;

    return -1;
}

bool PluginStateStore::write (int slot)
{
    juce::MemoryBlock data;
    juce::String pluginPath;
    juce::String pluginId;
    juce::String pluginName;
    std::uint64_t revision = 0;
    juce::File currentDirectory;

    {
        const std::lock_guard<std::mutex> lock (mutex);

        currentDirectory = directory;

        if (slot < 0 || slot >= static_cast<int> (states.size()))
            return false;

        const auto& state = states[static_cast<std::size_t> (slot)];

        if (state.data.getSize() == 0)
            return false;

        data = state.data;
        pluginPath = state.pluginPath;
        pluginId = state.pluginId;
        pluginName = state.pluginName;
        revision = state.revision;
    }

    if (currentDirectory == juce::File())
        return true; // in-memory only

    return writePluginStateFile (pluginStateFileFor (currentDirectory, slot),
                                 slot,
                                 data,
                                 pluginPath,
                                 pluginId,
                                 pluginName,
                                 revision);
}

bool PluginStateStore::readFileLocked (int slot, const juce::File& file)
{
    PluginStateFile stored;

    if (! readPluginStateFileData (file, slot, stored))
        return false;

    auto& state = states[static_cast<std::size_t> (slot)];
    state.data = std::move (stored.data);
    state.revision = stored.revision;
    state.appliedRevision = 0;
    state.pluginPath = stored.pluginPath;
    state.pluginId = stored.pluginId;
    state.pluginName = stored.pluginName;
    return true;
}

} // namespace ihub::session
