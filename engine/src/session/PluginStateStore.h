#pragma once

#include <juce_core/juce_core.h>

#include <cstdint>
#include <mutex>
#include <vector>

namespace AudioEngine
{
class SlotSource;
}

namespace ihub::plugins
{
class PluginInstrument;
}

namespace ihub::session
{

struct PluginStateIdentity
{
    juce::String pluginPath;
    juce::String pluginId;
    juce::String pluginName;
};

class PluginStateStore
{
public:
    explicit PluginStateStore (int slotCapacity = 64);

    void setDirectory (const juce::File& directory);
    juce::File getDirectory() const;

    bool loadAll();

    bool saveAll();

    bool capture (int slot, const PluginStateIdentity& identity);

    bool storeState (int slot,
                     const PluginStateIdentity& identity,
                     const juce::MemoryBlock& data);

    juce::MemoryBlock readState (int slot,
                                 const PluginStateIdentity& identity) const;

    void apply (int slot,
                plugins::PluginInstrument& instrument,
                const PluginStateIdentity& identity,
                bool force = false);

    void applyToRegistered (int slot, const PluginStateIdentity& identity);

    void clear (int slot);

    bool hasState (int slot) const noexcept;
    juce::int64 getStateSize (int slot) const noexcept;

    void registerInstrument (int slot, plugins::PluginInstrument& instrument) noexcept;
    void unregisterInstrument (plugins::PluginInstrument& instrument) noexcept;

    plugins::PluginInstrument* getInstrument (int slot) const noexcept;
    int findSlotForInstrument (plugins::PluginInstrument& instrument) const noexcept;

private:
    struct StoredState
    {
        juce::MemoryBlock data;
        std::uint64_t revision = 0;
        std::uint64_t appliedRevision = 0;
        juce::String pluginPath;
        juce::String pluginId;
        juce::String pluginName;
    };

    bool write (int slot);
    bool readFileLocked (int slot, const juce::File& file);

    mutable std::mutex mutex;
    juce::File directory;
    std::vector<StoredState> states;
    std::vector<plugins::PluginInstrument*> instruments;
};

}
