#pragma once

#include "PadState.h"

#include <array>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Controller
{

enum class MappingLayout
{
    families,
    tracks
};

enum class PadRole
{
    patch,
    track,
    fx,
    volume
};

struct TrackSpec
{
    std::string id;
    std::string name;
    int slot = -1;
    std::vector<std::string> hotFx;

    bool operator== (const TrackSpec&) const = default;
};

struct PatchFxSpec
{
    std::string kind = "builtin";
    std::string id;
    std::string name;
    std::string pluginState;
    bool bypassed = false;
    std::map<std::string, float> parameters;

    bool operator== (const PatchFxSpec&) const = default;
};

struct InstrumentSpec
{
    std::string id;
    std::string name;
    int slot = -1;

    std::string kind = "empty";

    std::string pluginFormat = "VST3";
    std::string pluginId;
    std::string pluginName;
    std::string pluginManufacturer;
    std::string pluginVersion;
    std::string pluginState;

    std::string samplePath;

    float volumeDb = 0.0f;

    float pan = 0.0f;

    bool muted = false;
    bool solo = false;

    static constexpr int inheritColorIndex = -1;

    int colorIndex = inheritColorIndex;

    int resolvedColorIndex = 0;

    std::vector<PatchFxSpec> fxChain;

    bool operator== (const InstrumentSpec&) const = default;
};

struct FamilySpec
{
    std::string id;
    std::string name;

    int colorIndex = 0;

    std::vector<InstrumentSpec> instruments;

    const InstrumentSpec* findInstrument (const std::string& instrumentId) const noexcept;
};

struct MacroSpec
{
    std::string id;
    std::string name;
    std::string parameter;
    float minimum = 0.0f;
    float maximum = 1.0f;
    float defaultValue = 0.0f;
};

struct PadBinding
{
    int pad = -1;
    PadRole role = PadRole::patch;
    std::string family;
    std::string instrument;
    std::string track;
    int slot = -1;
    int hot = -1;
    int step = -1;

    bool operator== (const PadBinding&) const = default;
};

struct EncoderBinding
{
    int encoder = -1;
    std::string macro;

    bool operator== (const EncoderBinding&) const = default;
};

struct MacroBankSpec
{
    int bank = -1;
    std::string name;
    std::vector<std::string> macros;

    bool operator== (const MacroBankSpec&) const = default;
};

struct SceneSpec
{
    int scene = -1;
    std::string name;
    std::string family;

    bool operator== (const SceneSpec&) const = default;
};

struct SetMapping
{
    static constexpr int supportedSchemaVersion = 1;
    static constexpr int supportedTrackSchemaVersion = 2;
    static constexpr int maximumSlot = 63;

    static constexpr int maximumColourIndex = 15;

    static constexpr int maximumPads = 64;

    static constexpr int maximumEncoders = 8;

    static constexpr int maximumBanks = 8;
    static constexpr int maximumScenes = 8;

    static constexpr int maximumTracks = 8;
    static constexpr int maximumHotFx = 2;
    static constexpr int maximumVolumeStep = 4;

    int schemaVersion = supportedSchemaVersion;
    MappingLayout layout = MappingLayout::families;
    std::string name;

    std::map<std::string, FamilySpec> families;
    std::map<std::string, MacroSpec> macros;
    std::vector<TrackSpec> tracks;

    std::array<std::optional<PadBinding>, maximumPads> pads;
    std::array<std::optional<EncoderBinding>, maximumEncoders> encoders;
    std::array<std::optional<MacroBankSpec>, maximumBanks> banks;
    std::array<std::optional<SceneSpec>, maximumScenes> scenes;

    const PadBinding* padBinding (int pad) const noexcept;
    const EncoderBinding* encoderBinding (int encoder) const noexcept;
    const MacroBankSpec* bankBinding (int bank) const noexcept;
    const SceneSpec* sceneBinding (int scene) const noexcept;
    const TrackSpec* trackAtSlot (int slot) const noexcept;
    const TrackSpec* trackForId (std::string_view id) const noexcept;
    const TrackSpec* trackFor (const PadBinding& binding) const noexcept;

    const InstrumentSpec* instrumentFor (const PadBinding& binding) const noexcept;
    const MacroSpec* macroFor (const EncoderBinding& binding) const noexcept;

    int colorIndexFor (const PadBinding& binding) const noexcept;

    int countPatches() const noexcept;

    int countPads() const noexcept;
    int countEncoders() const noexcept;
    int countBanks() const noexcept;
    int countScenes() const noexcept;
    int countTracks() const noexcept;

    std::vector<const InstrumentSpec*> allInstruments() const;
};

}
