#pragma once

#include "FxChain.h"

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <atomic>
#include <map>
#include <memory>
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

/**
    One effect in a patch's insert chain.

    It is a real `juce::AudioProcessor`, so `FxChain` stores it exactly like a
    hosted plugin. It owns the wrapped processor (a built-in DSP block or a
    hosted VST3) and adds two controls the chain itself does not have:

    - **Bypass**: an atomic flag. When set the wrapper returns without touching
      the buffer and reports zero latency, so bypassing a look-ahead effect
      immediately removes its delay from the chain total.
    - **Wet/dry**: an atomic 0..1 balance applied with a dry copy taken before
      the wrapped processor runs. `wet == 1` skips the blend and keeps the
      processed signal untouched; `wet == 0` is a bit-exact dry passthrough.

    Threading:

    - `prepareToPlay()`, `releaseResources()`, `reset()`, the parameter setters
      and the wrapped processor's own setters are message-thread calls.
    - `setBypassed()` / `setWet()` are lock-free atomic stores, so the API may
      change them between blocks without touching the audio thread contract.
    - `processBlock()` is the audio path: it only writes memory allocated in
      `prepareToPlay()`; no allocation, locks or logging.
*/
class InsertEffect final : public juce::AudioProcessor
{
public:
    InsertEffect (std::unique_ptr<juce::AudioProcessor> processorToWrap,
                  FxInsertType typeToUse,
                  std::string idToUse,
                  std::string nameToUse);
    ~InsertEffect() override = default;

    //==============================================================================
    // AudioProcessor

    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    void reset() override;

    using juce::AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    bool hasEditor() const override                 { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    const juce::String getName() const override     { return effectName; }
    bool acceptsMidi() const override               { return false; }
    bool producesMidi() const override              { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override                   { return 1; }
    int getCurrentProgram() override                { return 0; }
    void setCurrentProgram (int) override           {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Insert controls (lock-free)

    void setBypassed (bool shouldBeBypassed) noexcept;
    bool isBypassed() const noexcept;

    void setWet (float wetAmount) noexcept;
    float getWet() const noexcept;

    /** Re-reads the wrapped processor's latency, publishes it through
        `AudioProcessor::setLatencySamples()` (zero while bypassed or fully
        dry) and caches it. Off the audio thread. */
    void refreshLatency() noexcept;

    //==============================================================================
    juce::AudioProcessor* getWrappedProcessor() const noexcept { return processor.get(); }
    FxInsertType getType() const noexcept { return type; }
    const std::string& getId() const noexcept { return id; }

private:
    std::unique_ptr<juce::AudioProcessor> processor;
    const FxInsertType type;
    const std::string id;
    const juce::String effectName;

    std::atomic<bool> bypassed { false };
    std::atomic<float> wet { 1.0f };
    std::atomic<int> cachedLatency { 0 };

    juce::AudioBuffer<float> dry;
    int maximumBlockSize = 0;
    int numChannels = 2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InsertEffect)
};

/**
    The message-thread owner of one patch's insert chain.

    `Slot` keeps the actual `FxChain` (the audio path); this rack owns the
    processors it points at and rebuilds the chain after every edit. The two
    are kept separate so the API can keep a serialisable model (type, id, name,
    bypass, wet, parameters) without reaching into JUCE plugins.

    Editing rules:

    - All methods are message-thread calls.
    - The caller must ensure the audio thread is not inside the chain while
      `rebuild()` runs. `Session` gates the slot for one block before calling
      these methods (see `Session::beginFxEditLocked`).
    - A failed `add()` / `replace()` leaves the existing chain untouched.

    Built-in parameters, by type:

    - `eq`: `lowFrequencyHz`, `lowGainDb`, `lowQ`, `lowEnabled`,
      `midFrequencyHz`, `midGainDb`, `midQ`, `midEnabled`,
      `highFrequencyHz`, `highGainDb`, `highQ`, `highEnabled`
    - `compressor`: `thresholdDb`, `ratio`, `attackMs`, `releaseMs`, `makeupDb`
    - `delay`: `tempoSync` (0/1), `division` (0..9), `timeMs`, `feedback`,
      `dampingHz`
    - `reverb`: `roomSize`, `damping`, `width`, `preDelayMs`
    - `vst3`: parameter id or display name -> normalised 0..1 value
*/
class FxRack
{
public:
    explicit FxRack (juce::AudioPluginFormatManager* formatManagerToUse);
    ~FxRack() = default;

    //==============================================================================
    // Editing (message thread)

    /** Creates an insert and places it at `position` (clamped; -1 appends). */
    bool add (const FxInsertConfig& config,
              int position,
              double sampleRate,
              int maximumBlockSize,
              int numChannels,
              std::string& error);

    /** Replaces the whole chain atomically. A failure leaves it untouched. */
    bool replace (const std::vector<FxInsertConfig>& configs,
                  double sampleRate,
                  int maximumBlockSize,
                  int numChannels,
                  std::string& error);

    bool update (int index,
                 const FxInsertPatch& patch,
                 double sampleRate,
                 int maximumBlockSize,
                 int numChannels,
                 std::string& error);

    bool remove (int index, std::string& error);

    /** Reorders the chain. `order` must be a permutation of `0..size()-1`;
        `order[0]` becomes the first insert. */
    bool reorder (const std::vector<int>& order, std::string& error);

    void clear() noexcept;

    /** Copies the rack's processors into `chain`, in order. */
    void rebuild (FxChain& chain,
                  double sampleRate,
                  int maximumBlockSize,
                  int numChannels);

    //==============================================================================
    // Inspection

    int size() const noexcept { return static_cast<int> (inserts.size()); }
    int latencySamples() const noexcept;

    FxInsertSnapshot getInsert (int index) const;
    std::vector<FxInsertSnapshot> snapshots() const;

    //==============================================================================
    // Transport (tempo-synced delay)

    void setTransport (const FxTransport& transport) noexcept;
    const FxTransport& getTransport() const noexcept { return transport; }

private:
    struct Insert
    {
        std::unique_ptr<InsertEffect> effect;
        FxInsertType type = FxInsertType::equalizer;
        std::string id;
        std::string name;
    };

    bool create (const FxInsertConfig& config,
                 double sampleRate,
                 int maximumBlockSize,
                 int numChannels,
                 std::unique_ptr<InsertEffect>& destination,
                 std::string& error) const;

    bool applyParameters (InsertEffect& effect,
                          const std::map<std::string, float>& parameters,
                          bool replaceAll,
                          std::string& error) const;

    std::map<std::string, float> readParameters (const InsertEffect& effect) const;

    void applyTransportTo (InsertEffect& effect) const;

    juce::AudioPluginFormatManager* formatManager = nullptr;
    std::vector<Insert> inserts;
    FxTransport transport;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxRack)
};

} // namespace AudioEngine
