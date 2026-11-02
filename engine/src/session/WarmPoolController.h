#pragma once

#include "audio/WarmPool.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace AudioEngine
{
class AudioEngine;
class SlotSwitcher;
}

namespace ihub::session
{

class WarmPoolController
{
public:
    using SourceFactory = std::function<std::unique_ptr<AudioEngine::SlotSource> (
        int slot, juce::String& error)>;
    using DestroyObserver = std::function<void (AudioEngine::SlotSource& instance)>;
    using BeforeArmHook = std::function<void (int slot)>;
    using CancelLoads = std::function<void()>;

    WarmPoolController();
    ~WarmPoolController();

    void createVoices (int slotCount);

    void clearVoices() noexcept;

    AudioEngine::SlotSource* getVoiceSource (int slot) noexcept;
    int getNumVoices() const noexcept;

    void createPool (int neighbourCount);

    void configure (SourceFactory factory,
                    DestroyObserver destroyObserver,
                    BeforeArmHook beforeArm,
                    AudioEngine::SlotSwitcher* switcherToUse,
                    CancelLoads cancelLoads);

    void setFormat (double sampleRate, int maximumBlockSize);
    void setNumSlots (int slotCount);
    void setRequest (int slot, const juce::String& requestId);
    void setActiveSlot (int slot);
    void preloadAll();

    void shutdown() noexcept;

    void arm (int slot);

    void disarm();

    void resetArmState() noexcept;

    void replaceRequest (int slot,
                         const juce::String& requestId,
                         bool wasActive,
                         bool wasRequested);

    int getRequestedSlot() const noexcept;
    int getActiveSlot() const noexcept;
    int getPendingSlot() const noexcept;

    bool isWarm (int slot) const noexcept;
    bool isWarming (int slot) const noexcept;
    bool isActive (int slot) const noexcept;

    int getInstanceLimit() const noexcept;
    int getLiveInstrumentCount() const noexcept;
    int getPeakInstrumentCount() const noexcept;
    int getNeighbourCount() const noexcept;
    void setNeighbourCount (int count);

    bool waitForSlotWarm (int slot, int timeoutMs);
    bool waitForSlotActive (int slot, int timeoutMs);
    bool waitForIdle (int timeoutMs);

    AudioEngine::WarmPool* getPool() noexcept { return warmPool.get(); }
    const AudioEngine::WarmPool* getPool() const noexcept { return warmPool.get(); }

    bool pinPairForMeasurement (int fromSlot, int toSlot, int savedNeighbourCount);

    void bindVoice (int slot, AudioEngine::SlotSource* instance) noexcept;
    void clearVoice (AudioEngine::SlotSource* instance) noexcept;

private:
    void warmCallback (int slot);
    bool finishArmLocked (int slot, AudioEngine::SlotSource* pinnedInstance);
    bool warmSlotBlockingLocked (int slot, int timeoutMs);
    bool waitForWarmIdle (int timeoutMs);
    void releaseAllPinsLocked();
    void pumpMessages (int milliseconds);

    SourceFactory sourceFactory;
    DestroyObserver destroyObserver;
    BeforeArmHook beforeArmHook;
    CancelLoads cancelLoads;
    AudioEngine::SlotSwitcher* switcher = nullptr;

    std::unique_ptr<AudioEngine::WarmPool> warmPool;
    std::vector<std::unique_ptr<AudioEngine::SlotSource>> voices;

    mutable std::mutex armMutex;
    std::atomic<int> requestedSlot { -1 };
    std::atomic<int> activeSlot { -1 };
    std::atomic<int> pendingArmSlot { -1 };
    int pinnedActiveSlot = -1;
    int pinnedPreviousSlot = -1;
};

}
