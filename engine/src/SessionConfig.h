#pragma once

#include "audio/WarmPool.h"
#include "audio/fx/FxInsertTypes.h"

#include <juce_core/juce_core.h>

#include <string>
#include <vector>

namespace ihub
{

/** What produces audio for one slot of the composed session. */
enum class SessionSlotKind
{
    /** No instrument assigned: the slot renders silence.

        This is the default for a patch the user has not given a source yet.
        It is a real source, so the fixed graph and warm pool stay uniform, but
        it never generates audio and ignores MIDI.
    */
    empty,

    /** Built-in initialisation tone.

        Used by the offline tests and the `--session-smoke` diagnostic, which
        need a deterministic source without an external kit or plugin bundle.
    */
    initTone,

    /** `SamplerInstrument` over a kit JSON file (`path`). */
    sampler,

    /** `PluginInstrument` over a VST3 bundle (`path`). */
    plugin,

    /** `AudioInputInstrument` for direct monitoring of the interface. */
    audioInput
};

const char* toString (SessionSlotKind kind) noexcept;

/** Warm-pool state of one logical slot, exposed through `/state`. */
enum class SessionWarmState
{
    cold,
    warming,
    warm,
    active,
    fading
};

const char* toString (SessionWarmState state) noexcept;

/** One instrument to load into a fixed engine slot. */
struct SessionSlotConfig
{
    /** Display name of the patch. */
    std::string name;
    SessionSlotKind kind = SessionSlotKind::empty;

    /** Kit document (sampler) or VST3 bundle (plugin). */
    juce::String path;

    /** Frequency of the initialisation tone, in Hz. */
    double frequency = 220.0;

    /** Post-instrument gain, 0 .. 4. */
    float gain = 1.0f;

    /** Stereo balance, -1 (left) .. +1 (right). Published by the API; the
        graph has no panner yet. */
    float pan = 0.0f;

    /** Patch mute. Applied to the slot gain (muted patches are silent). */
    bool muted = false;

    /** Patch solo. When any patch is soloed, the others are silent. */
    bool solo = false;

    /** Resolved palette index, 0 .. 15. */
    int colorIndex = 0;

    /** Instrument descriptor published by `/state`. `kind` above is the audio
        source this session actually instantiates; a set may describe a plugin
        patch while the default session renders the initialisation tone. */
    std::string instrumentKind = "empty";
    std::string pluginFormat = "VST3";
    std::string pluginId;
    std::string pluginName;
    std::string pluginManufacturer;
    std::string pluginVersion;
    std::string samplePath;

    /** Interface channel copied by an audio-input slot. */
    int inputChannel = 0;

    /** Family and patch identity from the set document. Used by the API's
        family/patch tree; the audio path only cares about `kind`/`path`. */
    std::string family;
    std::string familyName;
    std::string patch;
    std::string patchName;

    /** Persisted insert chain, built into the fixed slot's FX chain. */
    std::vector<AudioEngine::FxInsertConfig> fxChain;
};

/** How and where the composed session opens its audio device. */
struct SessionDeviceConfig
{
    juce::String deviceTypeName;
    juce::String outputDeviceName;
    juce::String inputDeviceName;

    double sampleRate = 48000.0;
    int bufferSize = 128;
    int numOutputChannels = 2;
    int numInputChannels = 2;

    /** When true, only an ALSA `Loopback` output is accepted.

        Used by `--session-smoke` so the acceptance run cannot silently fall
        back to the desktop's default output device.
    */
    bool preferLoopback = false;
};

/** The complete default-session description. */
struct SessionConfig
{
    SessionDeviceConfig device;
    std::vector<SessionSlotConfig> slots;

    /** Equal-power crossfade length. 0 is the test seam for a hard switch. */
    float fadeMilliseconds = 3.0f;

    /** Slot to warm and arm around at startup. */
    int initialSlot = 0;

    /** Neighbours kept warm around the armed slot. The instance bound is
        `1 + neighbourCount` (clamped to at least one neighbour, so the
        previous patch can stay warm while a cold one loads). */
    int neighbourCount = AudioEngine::WarmPool::defaultNeighbourCount;

    /** Artificial delay before a cold patch is created, in milliseconds.

        Zero in production. The headless warm-pool acceptance uses it to make
        the `warming` -> `active` transition observable without a real plugin.
    */
    int loadDelayMilliseconds = 0;
};

} // namespace ihub
