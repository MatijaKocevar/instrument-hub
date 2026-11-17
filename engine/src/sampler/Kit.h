#pragma once

#include <juce_core/juce_core.h>

#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace ihub::sampler {

inline constexpr int kitSchemaVersion = 1;

inline constexpr int minimumMidiNote = 0;
inline constexpr int maximumMidiNote = 127;
inline constexpr int minimumMidiVelocity = 1;
inline constexpr int maximumMidiVelocity = 127;

inline constexpr int maximumKitPads = 128;
inline constexpr int maximumLayersPerPad = 32;
inline constexpr int maximumRoundRobinPerLayer = 64;
inline constexpr int maximumKitSampleReferences = 4096;
inline constexpr std::size_t maximumKitNameBytes = 256;
inline constexpr std::size_t maximumKitPadNameBytes = 256;
inline constexpr std::size_t maximumKitSamplePathBytes = 4096;

inline constexpr double maximumTuneSemitones = 48.0;

struct KitSampleRef {
    std::string path;

    float gain = 1.0f;

    float pan = 0.0f;

    double tuneSemitones = 0.0;

    double playbackRatio() const noexcept { return std::pow(2.0, tuneSemitones / 12.0); }

    bool operator==(const KitSampleRef&) const = default;
};

struct VelocityLayer {
    int minVelocity = minimumMidiVelocity;
    int maxVelocity = maximumMidiVelocity;

    std::vector<KitSampleRef> roundRobin;

    bool contains(int velocity) const noexcept {
        return velocity >= minVelocity && velocity <= maxVelocity;
    }

    bool operator==(const VelocityLayer&) const = default;
};

struct KitPad {
    int note = 36;
    std::string name;

    float gain = 1.0f;

    float pan = 0.0f;

    double tuneSemitones = 0.0;

    std::vector<VelocityLayer> layers;

    const VelocityLayer* layerForVelocity(int velocity) const noexcept;

    double playbackRatio() const noexcept { return std::pow(2.0, tuneSemitones / 12.0); }

    bool operator==(const KitPad&) const = default;
};

struct Kit {
    static constexpr int currentVersion = kitSchemaVersion;

    int schemaVersion = currentVersion;
    std::string name;
    std::vector<KitPad> pads;

    bool operator==(const Kit&) const = default;

    const KitPad* findPad(int note) const noexcept;

    bool validate(juce::String& error) const;
};

bool isSafeSamplePath(std::string_view path) noexcept;

}  // namespace ihub::sampler
