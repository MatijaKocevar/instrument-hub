#pragma once

#include "audio/AudioEngine.h"
#include "audio/DeviceManager.h"
#include "audio/SlotSwitcher.h"
#include "instruments/Instrument.h"

#include <juce_audio_devices/juce_audio_devices.h>

#include <memory>
#include <string>
#include <vector>

namespace juce
{
class AudioPluginFormatManager;
}

namespace ihub::sampler
{
class LoadedKit;
}

namespace ihub
{

/** What produces audio for one slot of the composed session. */
enum class SessionSlotKind
{
    /** Built-in initialisation tone.

        The default set has no external samples or plugin bundles to rely on,
        so every slot starts with a phase-continuous test tone. It is a real
        `Instrument` / `AudioEngine::SlotSource` and follows the same audio
        thread rules as the sampler and plugin sources.
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

/** One instrument to load into a fixed engine slot. */
struct SessionSlotConfig
{
    std::string name;
    SessionSlotKind kind = SessionSlotKind::initTone;

    /** Kit document (sampler) or VST3 bundle (plugin). */
    juce::String path;

    /** Frequency of the initialisation tone, in Hz. */
    double frequency = 220.0;

    /** Post-instrument gain, 0 .. 4. */
    float gain = 1.0f;

    /** Interface channel copied by an audio-input slot. */
    int inputChannel = 0;
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
};

/** Measurements from an offline armed-slot / slot-switch render. */
struct SessionSwitchReport
{
    int fromSlot = -1;
    int toSlot = -1;

    /** RMS of the armed slot before the switch, linear and in dBFS. */
    double rms = 0.0;
    double rmsDbfs = -1000.0;

    /** Largest sample-to-sample step seen anywhere in the measured span.

        This is the signal's ordinary slew plus any discontinuity; it is
        reported for context, not used as the click decision.
    */
    double maxSampleDelta = 0.0;
    double maxSampleDeltaDbfs = -1000.0;

    /** Largest discontinuity across the switch.

        The rendered switch is compared with the ideal equal-power crossfade
        of the same two source signals: each source is rendered in isolation
        from the same prepared state, then blended with the 3 ms product law.
        The value is the peak absolute difference between the real session
        output and that reference, in dBFS. A click, a hard switch or a broken
        ramp shows up as a large residual; ordinary signal curvature cancels.
    */
    double discontinuity = 0.0;
    double discontinuityDbfs = -1000.0;

    int samplesMeasured = 0;
    bool rendered = false;
};

/**
    The composed audio session.

    Owns the fixed `AudioEngine`, one `SlotSwitcher` over its eight slots, the
    instrument instances loaded into them and the `DeviceManager` that streams
    the graph to hardware. The wiring is done once in `build()`; nothing is
    ever rewired at runtime. `armSlot()` only publishes a lock-free request and
    `SlotSwitcher` runs the 2-5 ms equal-power crossfade on the audio thread.

    ## Default set

    `makeDefaultSet()` describes eight slots. With no external assets present
    each slot gets the built-in initialisation tone, so a fresh checkout makes
    sound immediately. `build()` also accepts sampler, plugin and audio-input
    slots, created through the existing `SamplerInstrument`,
    `PluginInstrument` and `AudioInputInstrument` implementations.

    ## Threading

    - `build()`, `prepare()`, `openDevice()`, `closeDevice()`, `resetGraph()`
      and `measureSwitch()` are message-thread calls; the stream must be
      stopped for the graph-editing ones.
    - `armSlot()` and the getters are lock-free and safe while the stream runs.
    - `audioDeviceIOCallbackWithContext()` and `render()` are the audio path:
      no allocation, locks, logging or file I/O. All buffers are allocated in
      `prepare()` at the maximum block size.
*/
class Session final : public juce::AudioIODeviceCallback
{
public:
    Session();
    ~Session() override;

    //==============================================================================
    // Composition (message thread)

    /** The eight-slot starter set.

        Every slot starts as an initialisation tone at a different pitch, so
        the composed session is audible without a plugin bundle or sample
        library. Each entry names its slot; load a set later to replace a tone
        with a sampler, plugin or audio-input source.
    */
    static std::vector<SessionSlotConfig> makeDefaultSet();

    /** Builds the graph and loads every configured source.

        Must run with the stream stopped. An empty `config.slots` uses
        `makeDefaultSet()`. On failure nothing is left half-built: the graph is
        released and `error` explains which slot failed.
    */
    bool build (const SessionConfig& config, juce::String& error);

    /** Sizes every buffer and prepares the slots and their sources.

        Called by `build()` and again from `audioDeviceAboutToStart()` when the
        device format changes.
    */
    void prepare (double sampleRate, int maximumBlockSize, int numChannels);

    /** Returns every slot to idle and drops the pending arm request.

        The stream must be stopped.
    */
    void resetGraph() noexcept;

    //==============================================================================
    // Device (message thread)

    /** Opens the configured output device and registers this session as its
        callback. `device.preferLoopback` restricts the search to the ALSA
        Loopback used by the hardware-free tests.
    */
    bool openDevice (juce::String* errorMessage = nullptr);

    void closeDevice();

    bool isDeviceOpen() const noexcept { return deviceOpen; }

    AudioEngine::DeviceManager& getDevices() noexcept { return devices; }

    //==============================================================================
    // Graph inspection

    bool isPrepared() const noexcept { return prepared; }
    int getNumSlots() const noexcept;

    AudioEngine::AudioEngine& getEngine() noexcept;
    const AudioEngine::AudioEngine& getEngine() const noexcept;

    AudioEngine::SlotSwitcher& getSwitcher() noexcept;
    const AudioEngine::SlotSwitcher& getSwitcher() const noexcept;

    const SessionConfig& getConfig() const noexcept { return config; }
    const std::vector<SessionSlotConfig>& getSlotConfigs() const noexcept
    {
        return config.slots;
    }

    /** The slot's total instrument + FX latency, in samples. */
    int getSlotLatencySamples (int slot) const noexcept;

    //==============================================================================
    // Control (lock-free)

    /** Requests an equal-power fade to `slot` at the next block boundary. */
    void armSlot (int slot) noexcept;

    int getArmedSlot() const noexcept;

    //==============================================================================
    // Audio thread / offline rendering

    /** Renders `numSamples` of the session into `output`.

        Real-time safe after `prepare()`. Used by the offline smoke render; the
        device callback follows the same path.
    */
    void render (juce::AudioBuffer<float>& output,
                 juce::MidiBuffer& midi,
                 int numSamples) noexcept;

    /** Renders a deterministic arm / switch / measure pass.

        Runs with the stream stopped: resets the graph, arms `fromSlot`, lets
        its fade-in settle, measures its RMS, switches to `toSlot`, and compares
        the rendered switch with the ideal 3 ms equal-power crossfade of the
        same two sources. `rmsDbfs` and `discontinuityDbfs` are relative to
        full scale.
    */
    SessionSwitchReport measureSwitch (int fromSlot,
                                       int toSlot,
                                       int settleBlocks = 8,
                                       int rmsBlocks = 8,
                                       int switchBlocks = 32);

    //==============================================================================
    // juce::AudioIODeviceCallback

    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                           int numInputChannels,
                                           float* const* outputChannelData,
                                           int numOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override;

private:
    AudioEngine::SlotSource* createSource (const SessionSlotConfig& slotConfig,
                                           int slotIndex,
                                           juce::String& error);
    bool resolveDevice (AudioEngine::DeviceSetup& setup, juce::String& error);
    void renderInternal (int numSamples, juce::MidiBuffer& midi) noexcept;
    void copyToOutput (float* const* outputChannelData,
                       int numOutputChannels,
                       int numSamples) noexcept;

    SessionConfig config;

    // Destruction order matters: the plugin format manager and the loaded kits
    // must outlive the instruments that reference them, and the engine must be
    // destroyed before the sources its slots point at.
    std::unique_ptr<juce::AudioPluginFormatManager> pluginFormats;
    std::vector<std::unique_ptr<sampler::LoadedKit>> kits;
    std::vector<std::unique_ptr<Instrument>> instruments;
    std::vector<AudioEngine::SlotSource*> sources;

    std::unique_ptr<AudioEngine::AudioEngine> engine;
    std::unique_ptr<AudioEngine::SlotSwitcher> switcher;
    AudioEngine::DeviceManager devices;

    juce::AudioBuffer<float> renderBuffer;
    juce::MidiBuffer blockMidi;

    double sampleRate = 48000.0;
    int maximumBlockSize = 0;
    int numChannels = 2;
    bool prepared = false;
    bool deviceOpen = false;
    bool callbackInstalled = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Session)
};

} // namespace ihub
