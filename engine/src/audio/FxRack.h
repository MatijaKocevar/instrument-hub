#pragma once

#include "FxChain.h"
#include "fx/FxInsertTypes.h"
#include "fx/InsertEffect.h"

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace AudioEngine
{

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

    /** Moves one insert to `to`. The caller validates both indices. */
    void moveInsert (int from, int to) noexcept;

    void applyTransportTo (InsertEffect& effect) const;

    juce::AudioPluginFormatManager* formatManager = nullptr;
    std::vector<Insert> inserts;
    FxTransport transport;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxRack)
};

} // namespace AudioEngine
