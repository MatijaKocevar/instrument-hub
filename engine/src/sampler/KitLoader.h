#pragma once

#include "Kit.h"
#include "SampleLoader.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace ihub::sampler
{

/** Constant-power pan gains, both already multiplied by the hit's gain. */
float panGainLeft (float pan, float gain = 1.0f) noexcept;
float panGainRight (float pan, float gain = 1.0f) noexcept;

/** One concrete sample chosen for a note and velocity.

    Returned by `LoadedKit::resolve()`. It is a small value type: the audio
    thread can copy it, read the pointers and the already-combined gain, pan
    and playback rate, and start a voice without any further lookup or
    allocation.
*/
struct ResolvedHit
{
    /** Decoded sample at the engine rate. Owned by the `LoadedKit`. */
    const SampleData* sample = nullptr;

    /** The pad and layer the sample came from. Owned by the `LoadedKit`. */
    const KitPad* pad = nullptr;
    const VelocityLayer* layer = nullptr;

    int note = 0;
    int velocity = 0;

    /** Index of the round-robin variant that was chosen. */
    int variant = 0;

    /** Pad gain multiplied by the sample gain (linear, not dB). */
    float gain = 1.0f;

    /** Pad pan plus sample pan, clamped to -1 .. 1. */
    float pan = 0.0f;

    /** Pad tune plus sample tune, as a playback-rate multiplier. */
    double playbackRatio = 1.0;

    bool isValid() const noexcept { return sample != nullptr && ! sample->isEmpty(); }

    float leftGain() const noexcept { return panGainLeft (pan, gain); }
    float rightGain() const noexcept { return panGainRight (pan, gain); }
};

/**
    A parsed kit with every referenced sample already decoded at the engine
    rate, plus the per-layer round-robin positions.

    Build one with `KitLoader` and keep it alive for as long as voices may
    reference its samples. The object is immutable after loading except for
    the atomic round-robin counters, so:

    - `resolve()` is safe from the audio thread. It performs no allocation,
      no locking, no logging and no file I/O; it only reads the prepared
      tables and increments one atomic counter.
    - `resolveWithVariant()` is also safe from the audio thread and does not
      touch the counters, which makes it useful for previews and tests.
    - Do not destroy a `LoadedKit` while a voice still points into it. The
      engine switches kits with the stream stopped (or after all voices are
      idle), exactly like `SampleHandoff`.

    `getKit()` and the inspection methods are for control threads.
*/
class LoadedKit final
{
public:
    LoadedKit() = default;
    ~LoadedKit() = default;

    LoadedKit (const LoadedKit&) = delete;
    LoadedKit& operator= (const LoadedKit&) = delete;

    LoadedKit (LoadedKit&&) noexcept = default;
    LoadedKit& operator= (LoadedKit&&) noexcept = default;

    /** True once a kit and its samples have been installed. */
    bool isValid() const noexcept { return ! kit.pads.empty(); }

    /** The parsed kit document. */
    const Kit& getKit() const noexcept { return kit; }

    /** Unique decoded sample files (identical paths are decoded once). */
    std::size_t getNumLoadedSamples() const noexcept { return samples.size(); }

    /** A unique decoded sample by index, or nullptr. For tests and previews. */
    const SampleData* getLoadedSample (std::size_t index) const noexcept
    {
        return index < samples.size() ? samples[index].get() : nullptr;
    }

    /** Picks the layer for `velocity` and the next round-robin variant.

        Returns an invalid hit when no pad has this note or no layer covers
        the velocity (which validation already rejects for a valid kit).
        Safe from the audio thread.
    */
    ResolvedHit resolve (int note, int velocity) noexcept;

    /** Like `resolve()`, but selects a specific round-robin variant without
        advancing the counter. `variantIndex` wraps. Safe from the audio
        thread.
    */
    ResolvedHit resolveWithVariant (int note, int velocity, int variantIndex) const noexcept;

private:
    friend class KitLoader;

    struct LayerBinding
    {
        std::size_t padIndex = 0;
        std::size_t layerIndex = 0;
        std::size_t firstSample = 0;
        std::size_t sampleCount = 0;
    };

    struct PadBinding
    {
        int note = 0;
        std::size_t firstLayer = 0;
        std::size_t numLayers = 0;
    };

    int findPadBinding (int note) const noexcept;
    ResolvedHit makeHit (std::size_t layerBindingIndex, int variant,
                         int note, int velocity) const noexcept;

    Kit kit;

    /** Unique decoded files, in first-reference order. */
    std::vector<std::shared_ptr<const SampleData>> samples;

    /** One pointer per sample reference, in traversal order. */
    std::vector<const SampleData*> samplePointers;

    /** Flattened layers in pad-then-layer order. */
    std::vector<LayerBinding> layerBindings;

    /** Pads in document order; each owns a contiguous range of layers. */
    std::vector<PadBinding> padBindings;

    /** One round-robin counter per flattened layer. Allocated at load. */
    std::unique_ptr<std::atomic<std::uint32_t>[]> roundRobinCounters;
    std::size_t numRoundRobinCounters = 0;
};

/**
    Reads, validates and decodes a JSON kit.

    ## Document

        {
          "schemaVersion": 1,
          "name": "Studio Kit",
          "pads": [
            {
              "note": 36,
              "name": "Kick",
              "gain": 1.0,
              "pan": 0.0,
              "tune": 0.0,
              "layers": [
                {
                  "minVelocity": 1,
                  "maxVelocity": 63,
                  "roundRobin": [
                    { "path": "kick-soft-1.wav" },
                    { "path": "kick-soft-2.wav" }
                  ]
                },
                {
                  "minVelocity": 64,
                  "maxVelocity": 127,
                  "roundRobin": [ { "path": "kick-hard.wav" } ]
                }
              ]
            }
          ]
        }

    Sample paths are relative to the kit file's directory unless absolute.
    The schema lives at `shared/schema/kit.schema.json`.

    ## Threading

    All loading functions decode synchronously on the calling thread and are
    for control threads only. `LoadedKit` is published to the audio thread
    only after it is fully built. A failed load leaves `destination`
    untouched.
*/
class KitLoader final
{
public:
    KitLoader() = delete;

    /** Largest kit document accepted, before parsing. */
    static constexpr juce::int64 maximumKitBytes = 1024 * 1024;

    /** Parses and validates `json` without reading any sample file. */
    static bool parse (const juce::String& json, Kit& destination, juce::String& error);

    /** Parses, validates and decodes every referenced sample.

        `baseDirectory` resolves relative sample paths. Returns false and
        fills `error` on a malformed document, a validation failure, or a
        missing/unreadable sample.
    */
    static bool loadFromJson (const juce::String& json,
                              const juce::File& baseDirectory,
                              double engineSampleRate,
                              LoadedKit& destination,
                              juce::String& error);

    /** Reads a kit file and decodes its samples relative to its directory. */
    static bool loadFromFile (const juce::File& file,
                              double engineSampleRate,
                              LoadedKit& destination,
                              juce::String& error);

private:
    static bool buildLoadedKit (const Kit& kit,
                                const juce::File& baseDirectory,
                                double engineSampleRate,
                                LoadedKit& destination,
                                juce::String& error);
};

} // namespace ihub::sampler
