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

/** The plugin identity a saved patch blob belongs to. */
struct PluginStateIdentity
{
    juce::String pluginPath;
    juce::String pluginId;
    juce::String pluginName;
};

/**
    Per-patch plugin state, captured to a sidecar next to `set.yaml`.

    One opaque blob per patch (`slot-<n>.bin`) holds the hosted plugin's
    `getStateInformation()` bytes in a small binary container. The blob is
    never inlined in JSON (plan/DECISIONS.md), and `revision` /
    `appliedRevision` make "restore when armed again" a real operation instead
    of blindly replacing a live edit.

    The store also keeps the live-instrument registry the warm-pool factory
    fills and its destroy hook clears. Both are guarded by one mutex, so a
    capture never races the instance it reads.
*/
class PluginStateStore
{
public:
    explicit PluginStateStore (int slotCapacity = 64);

    //==============================================================================
    // Sidecar directory (control thread)

    void setDirectory (const juce::File& directory);
    juce::File getDirectory() const;

    /** Re-reads every `slot-<n>.bin` from the current directory. */
    bool loadAll();

    /** Writes every in-memory patch blob to the current directory. */
    bool saveAll();

    //==============================================================================
    // Capture and restore (control thread)

    /** Captures the live plugin's state and writes the sidecar when it moved.

        Returns false when the slot has no resident plugin instance.
    */
    bool capture (int slot, const PluginStateIdentity& identity);

    /** Restores a stored blob into a plugin instance.

        `force` is used for a brand-new warm-pool instance; without it a blob
        that is already applied to the resident instance is left alone.
    */
    void apply (int slot,
                plugins::PluginInstrument& instrument,
                const PluginStateIdentity& identity,
                bool force = false);

    /** Applies the stored blob to the slot's currently registered instance. */
    void applyToRegistered (int slot, const PluginStateIdentity& identity);

    /** Drops the in-memory blob and deletes its sidecar file. */
    void clear (int slot);

    bool hasState (int slot) const noexcept;
    juce::int64 getStateSize (int slot) const noexcept;

    //==============================================================================
    // Live instrument registry

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

} // namespace ihub::session
