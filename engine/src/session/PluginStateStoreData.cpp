#include "PluginStateStore.h"

#include "PluginStateFile.h"

namespace ihub::session
{
bool PluginStateStore::seedState (int slot,
                                  const PluginStateIdentity& identity,
                                  const juce::MemoryBlock& data)
{
    if (slot < 0 || slot >= static_cast<int> (states.size())
        || data.getSize() == 0)
        return false;

    const std::lock_guard<std::mutex> lock (mutex);
    auto& state = states[static_cast<std::size_t> (slot)];

    if (state.data != data)
    {
        state.data = data;
        ++state.revision;
    }

    state.pluginPath = identity.pluginPath;
    state.pluginId = identity.pluginId;
    state.pluginName = identity.pluginName;
    state.appliedRevision = 0;
    return true;
}

bool PluginStateStore::storeState (int slot,
                                   const PluginStateIdentity& identity,
                                   const juce::MemoryBlock& data)
{
    if (slot < 0 || slot >= static_cast<int> (states.size())
        || data.getSize() == 0)
        return false;

    bool changed = false;
    juce::File currentDirectory;

    {
        const std::lock_guard<std::mutex> lock (mutex);

        currentDirectory = directory;

        auto& state = states[static_cast<std::size_t> (slot)];

        if (state.data != data)
        {
            state.data = data;
            ++state.revision;
            changed = true;
        }

        state.pluginPath = identity.pluginPath;
        state.pluginId = identity.pluginId;
        state.pluginName = identity.pluginName;
        state.appliedRevision = state.revision;
    }

    if (changed && currentDirectory != juce::File())
        return write (slot);

    return true;
}

juce::MemoryBlock PluginStateStore::readState (int slot,
                                               const PluginStateIdentity& identity) const
{
    if (slot < 0 || slot >= static_cast<int> (states.size()))
        return {};

    const std::lock_guard<std::mutex> lock (mutex);

    const auto& state = states[static_cast<std::size_t> (slot)];

    if (state.data.getSize() == 0)
        return {};

    PluginStateFileData stored;
    stored.pluginPath = state.pluginPath;
    stored.pluginId = state.pluginId;

    if (! pluginStateMatches (stored, identity.pluginPath, identity.pluginId))
        return {};

    return state.data;
}

}
