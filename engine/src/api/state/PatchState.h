#pragma once

#include <juce_core/juce_core.h>

#include <string>
#include <vector>

namespace Api
{

/** Static description of one audio slot. */
struct SlotSnapshot
{
    int index = 0;
    std::string name;
    bool armed = false;

    /** Audio-thread state of the fixed graph slot: idle, warming, active or
        fading. Supplied by a runtime provider when one is attached; "idle"
        otherwise.
    */
    std::string state = "idle";

    /** Warm-pool state: cold, warming, warm, active or fading. A cold patch is
        a descriptor only; `warming` means its instrument is being created. */
    std::string warmState = "cold";
    bool warm = false;

    /** Family/patch identity from the set document. */
    std::string family;
    std::string familyName;
    std::string patch;
    std::string patchName;

    /** Crossfade gain published by `SlotSwitcher`, 0 .. 1. */
    float gain = 0.0f;

    /** Instrument + FX latency of this slot's chain. */
    int latencySamples = 0;
    /** The same total at the sample rate it was measured at. */
    double latencyMs = 0.0;

    /** True while this slot's native editor is open (not serialised in `slots`). */
    bool editorOpen = false;
};

/** Instrument descriptor published with every patch in the family tree.

    The descriptor mirrors the patch's declared source. `POST
    /patches/{slot}/source` replaces it together with the live source the
    session instantiates. It is present for every patch, hot or cold, so the
    grid can show a plugin/sampler patch before its instance is loaded.
*/
struct PatchInstrumentState
{
    std::string id;
    std::string name;
    std::string kind = "empty";
    std::string pluginFormat = "VST3";
    std::string pluginId;
    std::string pluginName;
    std::string pluginManufacturer;
    std::string pluginVersion;
    std::string samplePath;
};

/** A requested instrument-source change for one patch (T124).

    Only `kind` is required. `path` is the kit document for `sampler` and the
    VST3 bundle for `plugin`; `frequency` exists for `init-tone` and
    `inputChannel` for `audio-input`. The descriptor fields are optional
    overrides the inventory supplies.
*/
struct PatchSourceRequest
{
    std::string kind = "empty";
    std::string name;
    std::string path;
    double frequency = 0.0;
    int inputChannel = 0;
    std::string pluginFormat = "VST3";
    std::string pluginId;
    std::string pluginName;
    std::string pluginManufacturer;
    std::string pluginVersion;
    std::string samplePath;
};

/** One patch in the family tree exposed by `GET /state`.

    Every patch carries its instrument descriptor, mixer state, resolved colour
    and live warm/latency values, so the UI can render the whole grid from one
    state document.
*/
struct PatchState
{
    int slot = -1;
    std::string id;
    std::string name;
    std::string family;
    std::string familyName;

    PatchInstrumentState instrument;

    /** Patch volume in dB, -60 .. +12. */
    double volumeDb = 0.0;
    /** Stereo balance, -1 (left) .. +1 (right). */
    double pan = 0.0;
    bool mute = false;
    bool solo = false;

    /** Resolved palette index, 0 .. 15 (family default or patch override). */
    int colorIndex = 0;

    /** Live warm-pool state: cold, warming, warm, active or fading. */
    std::string warmState = "cold";
    bool warm = false;

    /** True while the patch's native plugin editor window is on screen. */
    bool editorOpen = false;

    /** Instrument + FX latency of this patch's chain. */
    int latencySamples = 0;
    double latencyMs = 0.0;
};

/** One instrument family in the family tree. */
struct FamilyState
{
    std::string id;
    std::string name;
    std::vector<PatchState> patches;
};

/** Result of an explicit save of the running plugin's sound into a patch. */
struct PatchStateCapture
{
    int slot = -1;
    bool captured = false;

    /** Size of the patch's opaque state blob after the capture, in bytes. */
    juce::int64 sizeBytes = 0;
};

} // namespace Api
