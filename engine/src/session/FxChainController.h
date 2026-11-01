#pragma once

#include "audio/FxRack.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace juce
{
class AudioPluginFormatManager;
}

namespace AudioEngine
{
class AudioEngine;
}

namespace ihub
{
struct SessionSlotConfig;
}

namespace ihub::session
{

/**
    Owns the per-patch insert racks and serialises chain edits against the
    audio callback.

    The engine slot's `FxChain` only holds non-owning processor pointers, so
    the `FxRack` owns them. Every edit gates the target slot and waits for one
    rendered block (`noteBlockRendered()` is called at the end of the block),
    which proves the callback moved past the gate before the chain vector is
    rebuilt. The audio thread never takes the edit mutex.
*/
class FxChainController
{
public:
    FxChainController();
    ~FxChainController();

    /** Attaches the fixed engine the racks mirror. */
    void setEngine (AudioEngine::AudioEngine* engineToUse) noexcept;

    /** Stores the format used when a rack is rebuilt. */
    void setFormat (double sampleRate, int maximumBlockSize, int numChannels) noexcept;

    /** Builds every rack from the persisted session configuration. */
    void build (const std::vector<SessionSlotConfig>& slotConfigs,
                juce::AudioPluginFormatManager& formats,
                const AudioEngine::FxTransport& transport);

    /** Drops every rack. */
    void reset() noexcept;

    bool getSnapshot (int slot,
                      AudioEngine::FxChainSnapshot& snapshot,
                      std::string& error) const;

    bool add (int slot,
              const AudioEngine::FxInsertConfig& insert,
              int position,
              AudioEngine::FxChainSnapshot& snapshot,
              std::string& error,
              bool streamActive);

    bool replace (int slot,
                  const std::vector<AudioEngine::FxInsertConfig>& chain,
                  AudioEngine::FxChainSnapshot& snapshot,
                  std::string& error,
                  bool streamActive);

    bool update (int slot,
                 int index,
                 const AudioEngine::FxInsertPatch& patch,
                 AudioEngine::FxChainSnapshot& snapshot,
                 std::string& error,
                 bool streamActive);

    bool remove (int slot,
                 int index,
                 AudioEngine::FxChainSnapshot& snapshot,
                 std::string& error,
                 bool streamActive);

    bool reorder (int slot,
                  const std::vector<int>& order,
                  AudioEngine::FxChainSnapshot& snapshot,
                  std::string& error,
                  bool streamActive);

    bool clear (int slot,
                AudioEngine::FxChainSnapshot& snapshot,
                std::string& error,
                bool streamActive);

    /** Publishes one transport snapshot to every tempo-synced insert. */
    void updateTransport (const AudioEngine::FxTransport& transport) noexcept;

    /** Audio-thread call at the end of every rendered block. */
    void noteBlockRendered() noexcept
    {
        renderEpoch.fetch_add (1, std::memory_order_release);
    }

private:
    bool beginEdit (int slot, bool streamActive, std::string& error);
    void endEdit (int slot) noexcept;
    void rebuild (int slot);
    bool makeSnapshot (int slot, AudioEngine::FxChainSnapshot& snapshot) const;

    mutable std::mutex mutex;
    AudioEngine::AudioEngine* engine = nullptr;
    juce::AudioPluginFormatManager* formats = nullptr;
    std::vector<std::unique_ptr<AudioEngine::FxRack>> racks;

    double sampleRate = 48000.0;
    int maximumBlockSize = 128;
    int numChannels = 2;

    /** Incremented by the audio path after every rendered block. An FX edit
        waits for it to advance, which proves the callback moved past the gate. */
    std::atomic<std::uint64_t> renderEpoch { 0 };
};

} // namespace ihub::session
