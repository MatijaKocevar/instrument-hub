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
} // namespace AudioEngine

namespace ihub::session
{

/**
    Owns the warm-pool runtime and the arm hand-off.

    The fixed graph slot itself never changes its `SlotSource*`: each engine
    slot points at a permanent `Voice` proxy owned here. The actual patch
    instrument lives in the `WarmPool` and is published into the proxy with an
    atomic pointer store, so arming is a pointer hand-over plus the 3 ms
    crossfade.

    The controller also owns the pin bookkeeping (`active`, `previous` and the
    pending cold arm). `Session` keeps only the façade and the policy around
    it: which editor to close, which patch state to capture, and how to wire
    the graph. All control-thread methods take `armMutex`; the audio thread
    never calls any of them.
*/
class WarmPoolController
{
public:
    using SourceFactory = std::function<std::unique_ptr<AudioEngine::SlotSource> (
        int slot, juce::String& error)>;
    using DestroyObserver = std::function<void (AudioEngine::SlotSource& instance)>;
    using BeforeArmHook = std::function<void (int slot)>;

    WarmPoolController();
    ~WarmPoolController();

    //==============================================================================
    // Setup (message thread, stream stopped)

    /** Creates one permanent proxy per engine slot. */
    void createVoices (int slotCount);

    /** Drops the proxies. Call after the engine that pointed at them is gone. */
    void clearVoices() noexcept;

    AudioEngine::SlotSource* getVoiceSource (int slot) noexcept;
    int getNumVoices() const noexcept;

    void createPool (int neighbourCount);

    /** Installs the factory, destroy hook and warm callback, and the switcher
        the arm hand-off publishes to. */
    void configure (SourceFactory factory,
                    DestroyObserver destroyObserver,
                    BeforeArmHook beforeArm,
                    AudioEngine::SlotSwitcher* switcherToUse);

    void setFormat (double sampleRate, int maximumBlockSize);
    void setNumSlots (int slotCount);
    void setRequest (int slot, const juce::String& requestId);
    void setActiveSlot (int slot);
    void preloadAll();

    /** Clears the pool and waits for its posted deletions. Keeps the voices. */
    void shutdown() noexcept;

    //==============================================================================
    // Arm hand-off (control thread)

    /** Requests an equal-power fade to `slot`, warming it if necessary. */
    void arm (int slot);

    /** Fades everything out and clears the pending request. */
    void disarm();

    /** Drops every pin and forgets the pending/armed slot. */
    void resetArmState() noexcept;

    /** Replaces one request after a source change.

        Drops the slot's pins, retires the old instance and keeps the pending
        hand-off semantics for an active or still-warming slot.
    */
    void replaceRequest (int slot,
                         const juce::String& requestId,
                         bool wasActive,
                         bool wasRequested);

    int getRequestedSlot() const noexcept;
    int getActiveSlot() const noexcept;
    int getPendingSlot() const noexcept;

    //==============================================================================
    // Inspection and waits

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

    //==============================================================================
    // Measurement support

    /** Pins both endpoints of an offline switch measurement.

        The neighbourhood is temporarily reduced so the two pinned instances fit
        the `1 + K` budget, then restored to `savedNeighbourCount`. Mirrors the
        state the old in-session measurement set up directly.
    */
    bool pinPairForMeasurement (int fromSlot, int toSlot, int savedNeighbourCount);

    //==============================================================================
    // Proxy binding (control thread; the store is lock-free for the audio side)

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
    AudioEngine::SlotSwitcher* switcher = nullptr;

    std::unique_ptr<AudioEngine::WarmPool> warmPool;
    std::vector<std::unique_ptr<AudioEngine::SlotSource>> voices;

    // Warm-pool arm hand-off. The audio thread never touches these; the
    // switcher request itself is the only lock-free publish.
    mutable std::mutex armMutex;
    std::atomic<int> requestedSlot { -1 };
    std::atomic<int> activeSlot { -1 };
    std::atomic<int> pendingArmSlot { -1 };
    int pinnedActiveSlot = -1;
    int pinnedPreviousSlot = -1;
};

} // namespace ihub::session
