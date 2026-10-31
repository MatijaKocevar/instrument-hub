#pragma once

#include "Migrations.h"

#include <juce_core/juce_core.h>

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace AudioEngine
{
class AudioEngine;
}

namespace ihub::project
{

/** Directory inside the container that holds plugin state blobs. */
inline constexpr const char* pluginStateDirectory = "plugin-state";

/** Where a plugin state blob lives inside the container. */
std::string pluginStateEntryPath (std::string_view uuid);

/** True when `id` is safe to use as a container entry name.

    Only ASCII letters, digits, `-` and `_` are accepted. That is enough for
    UUIDs and for hand-written ids, and it makes path traversal impossible.
*/
bool isValidPluginStateId (std::string_view id) noexcept;

/** Generates a fresh UUID suitable for a plugin state chunk. */
std::string makePluginStateId();

/** True when `colour` is a `#rrggbb` hex string (ASCII, either case). */
bool isValidSlotColour (std::string_view colour) noexcept;

/** Longest accepted `SetFile::notes` text, in bytes. */
inline constexpr std::size_t maximumSetNotesBytes = 64 * 1024;

/** What produces audio for a slot. */
enum class SourceKind
{
    empty = 0,
    plugin,
    audioInput,
    sampler
};

const char* toString (SourceKind kind) noexcept;
bool sourceKindFromString (std::string_view text, SourceKind& kind) noexcept;

/** One instrument inside a slot. */
struct InstrumentConfig
{
    SourceKind kind = SourceKind::empty;

    /** Stable instrument id inside the set (for example "piano"). */
    std::string id;

    /** Display name. */
    std::string name;

    /** UUID of the plugin state chunk for `kind == plugin`. */
    std::string pluginState;

    /** Device input channel for `kind == audioInput`. */
    int inputChannel = 0;

    /** Post-instrument gain, 0 .. 4. */
    float gain = 1.0f;

    /** When true the slot skips its FX chain for direct monitoring. */
    bool directMonitor = false;

    bool operator== (const InstrumentConfig&) const = default;
};

/** How an FX chain entry is produced. */
enum class FxKind
{
    builtin = 0,
    plugin
};

const char* toString (FxKind kind) noexcept;
bool fxKindFromString (std::string_view text, FxKind& kind) noexcept;

/** One element of a slot's ordered FX chain. */
struct FxConfig
{
    FxKind kind = FxKind::builtin;

    /** Built-in effect id (for example "eq"), or the plugin id. */
    std::string id;

    /** Display name. */
    std::string name;

    /** UUID of the plugin state chunk for `kind == plugin`. */
    std::string pluginState;

    bool bypassed = false;

    /** Opaque, effect-specific parameter values. */
    std::map<std::string, float> parameters;

    bool operator== (const FxConfig&) const = default;
};

/** One fixed audio-graph slot. */
struct SlotConfig
{
    /** Fixed slot index in the engine, 0 .. 63. */
    int index = -1;

    /** Display name. */
    std::string name;

    /** Slot gain, 0 .. 4. A muted slot is applied as gain 0. */
    float gain = 1.0f;

    /** Grid colour as `#rrggbb`. Added by schema v2. */
    std::string colour = defaultSlotColour;

    bool active = false;
    bool muted = false;

    InstrumentConfig instrument;
    std::vector<FxConfig> fxChain;

    bool operator== (const SlotConfig&) const = default;
};

/** A plugin state blob and the metadata needed to find its plugin again.

    The blob is stored separately as `plugin-state/<uuid>.bin`; it is never
    inlined in `set.json`.
*/
struct PluginStateChunk
{
    std::string uuid;
    std::string format = "VST3";
    std::string pluginId;
    std::string name;
    std::string manufacturer;
    std::string version;

    juce::MemoryBlock data;

    bool operator== (const PluginStateChunk&) const = default;
};

/** Transport snapshot stored with the set. */
struct TransportConfig
{
    double bpm = 120.0;
    double positionPpq = 0.0;
    bool playing = false;

    bool operator== (const TransportConfig&) const = default;
};

/**
    The complete session as stored in a `.ihub` container.

    `set.json` holds this structure (minus the plugin blobs, which are listed
    by UUID), `mapping.yaml` holds the controller mapping verbatim, and
    `plugin-state/<uuid>.bin` holds each opaque plugin blob.

    The structure is deliberately plain data: it can be compared for deep
    equality, copied, and serialised without touching a live engine or loading
    a plugin. Binding a loaded set to actual instruments is the caller's job
    (`applyTo()` applies the graph-level settings that do not need a plugin
    instance; T081 adds the instance/state store).
*/
struct SetFile
{
    static constexpr int currentVersion = currentSetSchemaVersion;

    int schemaVersion = currentVersion;
    std::string name = "Untitled Set";

    /** Free-form set notes. Added by schema v2 (empty in a migrated v1 set). */
    std::string notes;

    std::vector<SlotConfig> slots;
    std::vector<PluginStateChunk> pluginStates;

    /** Raw contents of `mapping.yaml`. */
    std::string mappingYaml;

    TransportConfig transport;

    bool operator== (const SetFile&) const = default;

    /** Checks every reference and range without touching the filesystem.

        A set is valid when slot indices are unique, every plugin reference
        points at exactly one state chunk, plugin state ids are safe entry
        names, and numeric values are finite and in range.
    */
    bool validate (juce::String& error) const;

    /** The chunk with this UUID, or nullptr. */
    const PluginStateChunk* findPluginState (std::string_view uuid) const noexcept;
    PluginStateChunk* findPluginState (std::string_view uuid) noexcept;

    /** Applies the graph-level slot settings to a stopped engine.

        The engine must already own exactly the same number of slots as the
        set. Slot gain, mute and activation are applied; sources and FX chains
        are validated but not instantiated (that needs the plugin/filesystem
        layer, see T081). Returns false and fills `error` on mismatch.

        Do not call this while the engine is processing: `Slot::setGain()`,
        `Slot::activate()` and `Slot::deactivate()` are control-thread calls.
    */
    bool applyTo (AudioEngine::AudioEngine& engine, juce::String& error) const;
};

} // namespace ihub::project
