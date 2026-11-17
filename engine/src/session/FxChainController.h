#pragma once

#include "audio/FxRack.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace juce {
class AudioPluginFormatManager;
}

namespace AudioEngine {
class AudioEngine;
}

namespace ihub {
struct SessionSlotConfig;
}

namespace ihub::session {

class FxChainController {
public:
    FxChainController();
    ~FxChainController();

    void setEngine(AudioEngine::AudioEngine* engineToUse) noexcept;

    void setFormat(double sampleRate, int maximumBlockSize, int numChannels) noexcept;

    void build(const std::vector<SessionSlotConfig>& slotConfigs,
               juce::AudioPluginFormatManager& formats, const AudioEngine::FxTransport& transport);

    void reset() noexcept;

    bool getSnapshot(int slot, AudioEngine::FxChainSnapshot& snapshot, std::string& error) const;

    bool getTunerReading(int slot, int index, AudioEngine::FxTunerReading& reading,
                         std::string& error) const;

    bool add(int slot, const AudioEngine::FxInsertConfig& insert, int position,
             AudioEngine::FxChainSnapshot& snapshot, std::string& error, bool streamActive);

    bool replace(int slot, const std::vector<AudioEngine::FxInsertConfig>& chain,
                 AudioEngine::FxChainSnapshot& snapshot, std::string& error, bool streamActive);

    bool update(int slot, int index, const AudioEngine::FxInsertPatch& patch,
                AudioEngine::FxChainSnapshot& snapshot, std::string& error, bool streamActive);

    bool remove(int slot, int index, AudioEngine::FxChainSnapshot& snapshot, std::string& error,
                bool streamActive);

    bool reorder(int slot, const std::vector<int>& order, AudioEngine::FxChainSnapshot& snapshot,
                 std::string& error, bool streamActive);

    bool clear(int slot, AudioEngine::FxChainSnapshot& snapshot, std::string& error,
               bool streamActive);

    void updateTransport(const AudioEngine::FxTransport& transport) noexcept;

    void noteBlockRendered() noexcept { renderEpoch.fetch_add(1, std::memory_order_release); }

private:
    bool beginEdit(int slot, bool streamActive, std::string& error);
    void endEdit(int slot) noexcept;
    void rebuild(int slot);
    bool makeSnapshot(int slot, AudioEngine::FxChainSnapshot& snapshot) const;

    mutable std::mutex mutex;
    AudioEngine::AudioEngine* engine = nullptr;
    juce::AudioPluginFormatManager* formats = nullptr;
    std::vector<std::unique_ptr<AudioEngine::FxRack>> racks;

    double sampleRate = 48000.0;
    int maximumBlockSize = 128;
    int numChannels = 2;

    std::atomic<std::uint64_t> renderEpoch{0};
};

}  // namespace ihub::session
