#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace AudioEngine
{


/** The insert effects a patch's chain can hold.

    The four built-ins are the in-repo DSP blocks (`dsp::Eq`, `dsp::Compressor`,
    `dsp::Delay`, `dsp::Reverb`); `vst3` is any hosted plugin the format manager
    can instantiate in FX mode.
*/
enum class FxInsertType
{
    equalizer = 0,
    compressor,
    delay,
    reverb,
    vst3
};

const char* toString (FxInsertType type) noexcept;

/** Parses the API's stable type text: `eq`, `compressor`, `delay`, `reverb`,
    `vst3` (with `equalizer`, `comp` and `plugin` accepted as aliases).
*/
bool fxInsertTypeFromString (std::string_view text, FxInsertType& type) noexcept;

/** How one insert is created. Used by the persisted set and by `POST`. */
struct FxInsertConfig
{
    FxInsertType type = FxInsertType::equalizer;

    /** VST3 bundle path; empty for the built-ins. */
    std::string path;

    /** Stable identifier. Built-ins use their type name; a plugin uses its
        bundle path. */
    std::string id;

    std::string name;

    bool bypassed = false;

    /** Wet/dry balance, 0 (dry only) .. 1 (wet only). */
    float wet = 1.0f;

    /** Effect-specific values. See the README comment on `FxRack`. */
    std::map<std::string, float> parameters;
};

/** A partial update of one insert (`PATCH /patches/{slot}/fx/{index}`). */
struct FxInsertPatch
{
    std::optional<bool> bypassed;
    std::optional<float> wet;

    /** Moves the insert to this position in the chain. */
    std::optional<int> position;

    /** Replaces the effect's parameter values. */
    std::optional<std::map<std::string, float>> parameters;
};

/** One insert as published by `GET /patches/{slot}/fx`. */
struct FxInsertSnapshot
{
    int index = 0;
    FxInsertType type = FxInsertType::equalizer;

    /** `builtin` or `vst3`. */
    std::string kind;

    std::string id;
    std::string name;

    bool bypassed = false;
    float wet = 1.0f;

    /** Latency this insert contributes while it is in the audible path. */
    int latencySamples = 0;

    std::map<std::string, float> parameters;
};

/** A patch's complete insert chain plus its latency budget. */
struct FxChainSnapshot
{
    int slot = -1;

    /** Latency of the instrument source alone, in samples. */
    int instrumentLatencySamples = 0;

    /** Summed latency of the inserts that are in the audible path. */
    int fxLatencySamples = 0;

    /** `instrumentLatencySamples + fxLatencySamples`, in samples. */
    int latencySamples = 0;

    /** `latencySamples` at the engine's current sample rate. */
    double latencyMs = 0.0;

    std::vector<FxInsertSnapshot> inserts;
};

/** Transport values the tempo-synced delay locks to. */
struct FxTransport
{
    bool playing = false;
    double bpm = 120.0;
    double ppq = 0.0;
};

} // namespace AudioEngine
