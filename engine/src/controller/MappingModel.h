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
    tracks
};

enum class PadRole
{
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
    PadRole role = PadRole::track;
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

    bool operator== (const SceneSpec&) const = default;
};

struct SetMapping
{
    static constexpr int supportedSchemaVersion = 2;
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
    MappingLayout layout = MappingLayout::tracks;
    std::string name;

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

    const MacroSpec* macroFor (const EncoderBinding& binding) const noexcept;

    int countPads() const noexcept;
    int countEncoders() const noexcept;
    int countBanks() const noexcept;
    int countScenes() const noexcept;
    int countTracks() const noexcept;
};

}
