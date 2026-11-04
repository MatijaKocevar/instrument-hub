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

inline constexpr const char* pluginStateDirectory = "plugin-state";

std::string pluginStateEntryPath (std::string_view uuid);

bool isValidPluginStateId (std::string_view id) noexcept;

std::string makePluginStateId();

bool isValidSlotColour (std::string_view colour) noexcept;

inline constexpr std::size_t maximumSetNotesBytes = 64 * 1024;

inline constexpr int maximumSlotColourIndex = 15;

enum class SourceKind
{
    empty = 0,
    plugin,
    audioInput,
    sampler,
    initTone
};

const char* toString (SourceKind kind) noexcept;
bool sourceKindFromString (std::string_view text, SourceKind& kind) noexcept;

struct InstrumentConfig
{
    SourceKind kind = SourceKind::empty;

    std::string id;

    std::string name;

    std::string pluginState;

    std::string pluginFormat = "VST3";
    std::string pluginId;
    std::string pluginName;
    std::string pluginManufacturer;
    std::string pluginVersion;
    std::string pluginPath;

    std::string samplePath;

    int inputChannel = 0;

    float gain = 1.0f;

    bool directMonitor = false;

    bool operator== (const InstrumentConfig&) const = default;
};

enum class FxKind
{
    builtin = 0,
    plugin
};

const char* toString (FxKind kind) noexcept;
bool fxKindFromString (std::string_view text, FxKind& kind) noexcept;

struct FxConfig
{
    FxKind kind = FxKind::builtin;

    std::string type;

    std::string id;

    std::string name;

    std::string pluginState;

    bool bypassed = false;

    std::map<std::string, float> parameters;

    bool operator== (const FxConfig&) const = default;
};

struct SlotConfig
{
    int index = -1;

    std::string name;

    float gain = 1.0f;

    std::string colour = defaultSlotColour;

    int colorIndex = 0;

    float pan = 0.0f;

    bool solo = false;

    bool active = false;
    bool muted = false;

    InstrumentConfig instrument;
    std::vector<FxConfig> fxChain;

    bool operator== (const SlotConfig&) const = default;

    float volumeDb() const noexcept;
    void setVolumeDb (float decibels) noexcept;

    bool hasPluginState() const noexcept { return ! instrument.pluginState.empty(); }
};

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

struct TransportConfig
{
    double bpm = 120.0;
    double positionPpq = 0.0;
    bool playing = false;

    bool operator== (const TransportConfig&) const = default;
};

struct SetFile
{
    static constexpr int currentVersion = currentSetSchemaVersion;

    int schemaVersion = currentVersion;
    std::string name = "Untitled Set";

    std::string notes;

    std::vector<SlotConfig> slots;
    std::vector<PluginStateChunk> pluginStates;

    std::string mappingYaml;

    TransportConfig transport;

    bool operator== (const SetFile&) const = default;

    bool validate (juce::String& error) const;

    const PluginStateChunk* findPluginState (std::string_view uuid) const noexcept;
    PluginStateChunk* findPluginState (std::string_view uuid) noexcept;

    bool applyTo (AudioEngine::AudioEngine& engine, juce::String& error) const;
};

}
