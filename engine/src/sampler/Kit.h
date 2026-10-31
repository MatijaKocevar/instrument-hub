#pragma once

#include <juce_core/juce_core.h>

#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace ihub::sampler
{

/** Version of the JSON kit document understood by this build. */
inline constexpr int kitSchemaVersion = 1;

inline constexpr int minimumMidiNote = 0;
inline constexpr int maximumMidiNote = 127;
inline constexpr int minimumMidiVelocity = 1;
inline constexpr int maximumMidiVelocity = 127;

/** Structural limits, mirrored by shared/schema/kit.schema.json.

    They exist to bound memory before anything is decoded, and to keep a
    malformed document from turning into thousands of file reads.
*/
inline constexpr int maximumKitPads = 128;
inline constexpr int maximumLayersPerPad = 32;
inline constexpr int maximumRoundRobinPerLayer = 64;
inline constexpr int maximumKitSampleReferences = 4096;
inline constexpr std::size_t maximumKitNameBytes = 256;
inline constexpr std::size_t maximumKitPadNameBytes = 256;
inline constexpr std::size_t maximumKitSamplePathBytes = 4096;

/** Semitone range accepted for pad and sample tuning. */
inline constexpr double maximumTuneSemitones = 48.0;

/** One sample variant inside a velocity layer.

    The same structure is the round-robin unit: a layer with three of these
    rotates through them on consecutive hits. Gains are linear, pan is
    -1 (left) .. +1 (right), and tune is in semitones (positive is higher).
    The effective pad value is the sum/product of the pad and sample values;
    see `ResolvedHit` in `KitLoader.h`.
*/
struct KitSampleRef
{
    /** Absolute path, or a path relative to the kit file's directory. */
    std::string path;

    /** Linear gain, 0 .. 4. */
    float gain = 1.0f;

    /** Constant-power pan, -1 .. 1. */
    float pan = 0.0f;

    /** Tuning in semitones, -48 .. 48. */
    double tuneSemitones = 0.0;

    /** Playback-rate multiplier implied by `tuneSemitones`. */
    double playbackRatio() const noexcept
    {
        return std::pow (2.0, tuneSemitones / 12.0);
    }

    bool operator== (const KitSampleRef&) const = default;
};

/** A contiguous velocity range and its round-robin samples.

    A pad's layers must together cover every velocity from 1 to 127 exactly
    once. The ranges may be listed in any order; `Kit::validate()` sorts them
    while checking coverage.
*/
struct VelocityLayer
{
    int minVelocity = minimumMidiVelocity;
    int maxVelocity = maximumMidiVelocity;

    /** At least one variant; consecutive hits rotate through them. */
    std::vector<KitSampleRef> roundRobin;

    bool contains (int velocity) const noexcept
    {
        return velocity >= minVelocity && velocity <= maxVelocity;
    }

    bool operator== (const VelocityLayer&) const = default;
};

/** One pad, addressed by its MIDI note. */
struct KitPad
{
    int note = 36;
    std::string name;

    /** Linear pad gain, 0 .. 4, multiplied by the chosen sample's gain. */
    float gain = 1.0f;

    /** Pad pan, -1 .. 1, added to the chosen sample's pan and clamped. */
    float pan = 0.0f;

    /** Pad tuning in semitones, added to the chosen sample's tuning. */
    double tuneSemitones = 0.0;

    std::vector<VelocityLayer> layers;

    /** The layer that contains `velocity`, or nullptr when no layer does. */
    const VelocityLayer* layerForVelocity (int velocity) const noexcept;

    /** Playback-rate multiplier implied by `tuneSemitones`. */
    double playbackRatio() const noexcept
    {
        return std::pow (2.0, tuneSemitones / 12.0);
    }

    bool operator== (const KitPad&) const = default;
};

/** A complete kit definition, independent of any decoded audio.

    This is the parsed and validated form of `kit.json`. It is plain data:
    it can be copied, compared and serialised. Decoding the referenced files
    and binding them for playback is `KitLoader` / `LoadedKit`.

    The loader is strict about the document: unknown properties, duplicate
    pad notes, overlapping or missing velocity ranges, unsafe sample paths
    and out-of-range numbers are all rejected. `validate()` is the single
    source of truth for those rules and matches the JSON schema.
*/
struct Kit
{
    static constexpr int currentVersion = kitSchemaVersion;

    int schemaVersion = currentVersion;
    std::string name;
    std::vector<KitPad> pads;

    bool operator== (const Kit&) const = default;

    /** The pad with this MIDI note, or nullptr. */
    const KitPad* findPad (int note) const noexcept;

    /** Checks the whole document without touching the filesystem.

        Returns false and fills `error` with the first problem found.
    */
    bool validate (juce::String& error) const;
};

/** True when `path` is acceptable as a sample path.

    The path must be non-empty, bounded, free of NUL characters and free of
    `..` segments. Absolute paths are allowed; relative paths are resolved
    against the kit file's directory by `KitLoader`. This is not a sandbox
    (the engine may legitimately load any local sample), it only prevents a
    kit from silently escaping its own directory through traversal.
*/
bool isSafeSamplePath (std::string_view path) noexcept;

} // namespace ihub::sampler
