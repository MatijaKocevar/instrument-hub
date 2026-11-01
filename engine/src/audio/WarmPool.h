#pragma once

#include "Slot.h"

#include <any>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace AudioEngine
{

/**
    Warm pool of instrument instances (T033).

    Loading a plugin costs tens of milliseconds and allocates megabytes, so it
    must never happen on the path that arms a pad. `WarmPool` keeps the active
    slot and its `K` nearest neighbours instantiated, so arming one of them is
    a pointer hand-over plus a 3 ms crossfade.

    The implementation is split by concern under `audio/warm/`: slot
    reservation, warm-up state, eviction/accounting and the worker. This
    header is the public surface.

    ## What "neighbour" means

    Slots are an ordered set (`setNumSlots()`), one instance per slot. The
    *protected* set is the active slot plus the `K` slots nearest to it in that
    order, with lower indices breaking ties:

        slots 0..7, active 4, K = 2  ->  { 4, 3, 5 }

    The instance limit is exactly `1 + K` (three for the acceptance case of
    eight slots and `K = 2`). That limit is a hard invariant: the pool never
    holds more live instances than that, counting instances still waiting to
    be destroyed. A warm request that cannot fit is *deferred* and retried when
    a pin is released instead of quietly exceeding the budget.

    ## Threading

    - `setNumSlots()`, `setRequest()`, `setFactory()`, `setFormat()` and
      `clear()` are callback-thread calls. Nothing may process the slots while
      they run.
    - `setActiveSlot()`, `preloadAll()`, `acquire()`, `release()`, `armSlot()`
      and every getter are safe from any non-audio thread. They take a mutex;
      **never call them from `processBlock()`**.
    - The pool owns one background thread. It creates instances and decides
      what to evict, so neither a plugin load nor an LRU sweep ever runs on the
      caller's thread. `release()` only drops a pin.
    - Deletion is posted to the JUCE message thread, because JUCE's VST3
      wrapper calls `MessageManager::callSync` from its destructor. The pool
      counts an instance as live until that posted delete has actually run, so
      the instance limit holds across the hand-off. The message loop must be
      running; poll `isIdle()` rather than blocking it.
    - Nothing here touches the audio thread: the caller pins an instance with
      `acquire()`, calls `Slot::setSource()` while the stream is stopped, and
      releases the pin once the slot no longer processes.

    ## Lifetime

    `acquire()`/`armSlot()` return a borrowed pointer and increment a pin
    count. An instance is only destroyed when it is unpinned, so the pointer
    stays valid until `release()`. `getInstance()` is inspection only: it does
    **not** pin.

    Destroy the pool on the message thread, after `clear()` and
    `waitForIdle()`, so the last posted deletes have run.
*/
class WarmPool
{
public:
    using Instance = SlotSource;

    /** What to instantiate for one slot.

        `id` is the identity of the request (plugin file + state revision, for
        example). Re-setting the same id keeps a warm instance; a different id
        retires it. `payload` is opaque to the pool and handed to the factory,
        e.g. a `juce::PluginDescription` for a VST3.
    */
    struct Request
    {
        juce::String id;
        std::any payload;

        bool isValid() const noexcept { return id.isNotEmpty(); }
    };

    /** Result of `preloadAll()`, describing what the pool did at call time. */
    struct PreloadReport
    {
        int resident = 0; // already warm
        int queued = 0;   // load scheduled (or already in flight)
        int deferred = 0; // valid requests the instance limit cannot retain
        int failed = 0;   // factory errors from an earlier attempt
        juce::StringArray errors;

        bool isPending() const noexcept { return queued > 0; }
        bool isComplete() const noexcept { return ! isPending() && failed == 0; }
    };

    /** A snapshot of one slot, for the UI and for tests. */
    struct SlotInfo
    {
        int slot = -1;
        bool warm = false;
        bool protectedSlot = false;
        int pins = 0;
        juce::String id;
        juce::String lastError;
    };

    /** Creates one instance. Called on the pool's background thread, so it may
        load a plugin and allocate. Must return nullptr and fill `error` on
        failure; exceptions are caught by the pool. */
    using Factory = std::function<std::unique_ptr<Instance> (const Request&, juce::String& error)>;

    /** Runs immediately before an instance is destroyed, on the message
        thread. Close editors here. */
    using DestroyHook = std::function<void (Instance&)>;

    /** Called on the pool's background thread after an instance has become
        warm and is resident.

        The pool does not hold its mutex while calling this, so the hook may
        call `acquire()`/`release()`. It is the hand-off point a session uses
        to bind the instance to a fixed graph slot and to finish an arm that
        was waiting for a cold patch. */
    using WarmCallback = std::function<void (int slot)>;

    static constexpr int defaultNeighbourCount = 2;

    explicit WarmPool (int neighbourCount = defaultNeighbourCount);

    /** Stops the background thread and hands every remaining instance to the
        message thread for deletion. Call `clear()` and wait for `isIdle()`
        first, and destroy the pool on the message thread. */
    ~WarmPool();

    //==============================================================================
    // Setup (callback thread, slots must not be processing)

    void setFactory (Factory factory);
    void setDestroyHook (DestroyHook hook);
    void setWarmCallback (WarmCallback callback);

    /** Format handed to each instance right after it is created, so arming
        does not have to prepare it. Without this the caller prepares the
        instance through `Slot::setSource()` as usual. */
    void setFormat (double sampleRate, int maximumBlockSize);

    /** Number of neighbours kept warm around the active slot. */
    void setNeighbourCount (int count);
    int getNeighbourCount() const;

    /** Sizes the pool. Shrinking retires the instances of removed slots. */
    void setNumSlots (int count);
    int getNumSlots() const;

    /** Sets or replaces the request for a slot. Changing the id retires a warm
        instance and schedules a new one when the slot is protected. */
    void setRequest (int slot, Request request);

    /** The current request (copied). */
    Request getRequest (int slot) const;
    bool hasRequest (int slot) const;

    //==============================================================================
    // Control (any non-audio thread)

    /** Marks the active slot (-1 for none) and warms its neighbours in the
        background. Never blocks on a plugin load. */
    void setActiveSlot (int slot);
    int getActiveSlot() const;

    /** Pins the instance for `slot` and makes it active in one call.

        Returns nullptr when the slot is cold; that is the signal to fall back
        to a blocking load. The returned pointer must be handed to
        `release()` when the slot stops using it.
    */
    Instance* armSlot (int slot);

    /** Pins a warm instance, or returns nullptr. Never loads. */
    Instance* acquire (int slot);

    /** Drops a pin. The last unpinned, unprotected instance may be destroyed
        on the message thread soon after. */
    void release (int slot);

    /** Warms every request the instance limit can retain, nearest the active
        slot first.

        Non-blocking: it schedules the loads and returns immediately; poll
        `isIdle()` or `isWarm()` for completion. A preload never trades away
        the "at most active + K instances" invariant, so requests beyond the
        limit are reported as `deferred`.

        Plugin instantiation is deliberately kept off the caller's thread. JUCE
        posts the actual creation to the message thread and blocks the caller,
        so the message loop must keep running while a load is in flight.
    */
    PreloadReport preloadAll();

    //==============================================================================
    // Inspection (any non-audio thread)

    bool isWarm (int slot) const;

    /** True while this slot is queued for loading, deferred behind the
        instance cap, or being created on the background thread. */
    bool isWarming (int slot) const;

    /** Hard cap on live instances: `1 + K`, clamped to the slot count. */
    int getInstanceLimit() const;

    /** Instances currently alive, including ones queued for destruction. */
    int getLiveInstanceCount() const;

    /** Largest live-instance count observed since construction. This is the
        value that proves the instance bound held across a load hand-off. */
    int getPeakLiveInstanceCount() const;

    int getPinnedCount() const;

    /** Instances created successfully since construction. */
    std::uint64_t getWarmCount() const;

    /** Instances retired by LRU, request changes or resizing. */
    std::uint64_t getEvictionCount() const;

    /** The set the pool currently keeps warm. */
    std::vector<int> getProtectedSlots() const;

    std::vector<SlotInfo> describe() const;

    /** True when no load is in flight and no destruction is pending. */
    bool isIdle() const;

    /** Blocks until `isIdle()`. Never call this on the JUCE message thread
        while a load is in flight: JUCE posts creation to that thread, so the
        wait would never end. Poll `isIdle()` there instead. */
    bool waitForIdle (int timeoutMs = 10000);

    /** Retires every instance. Deletion is posted to the message thread; poll
        `isIdle()` to know when it finished. Requests are kept. */
    void clear();

private:
    struct SlotState
    {
        Request request;
        std::unique_ptr<Instance> instance;

        juce::String lastError;
        juce::String failedId; // do not retry the same failing request

        int pins = 0;
        std::uint64_t lastUsed = 0;

        bool retireAfterRelease = false;
        bool queued = false;   // already in warmQueue
        bool deferred = false; // waiting for a free slot under the cap
    };

    bool isValidSlotLocked (int slot) const noexcept { return slot >= 0 && slot < numSlots; }

    std::vector<int> protectedLocked() const;
    bool isProtectedLocked (int slot) const;
    int limitLocked() const;
    int liveCountLocked() const;
    bool isIdleLocked() const;

    bool needsWarmLocked (int slot) const;
    void enqueueWarmLocked (int slot);
    bool requeueDeferredLocked();
    bool evictOneLocked();

    void setActiveLocked (int slot);
    void retireLocked (int slot, bool force);

    /** Hands an instance to the message thread for destruction.

        JUCE's VST3 wrapper calls `MessageManager::callSync` from its
        destructor, so deleting a plugin on the pool's worker would block until
        the message loop runs. The delete is therefore posted, and the shared
        counter keeps the live-instance total honest until it has actually
        run. Safe to call without holding the mutex.
    */
    void scheduleDestroy (std::unique_ptr<Instance> instance, const DestroyHook& hook);

    std::unique_ptr<Instance> createInstance (const Request& request, juce::String& error);
    void run();

    void notifyWorker();

    mutable std::mutex mutex;
    std::condition_variable condition;

    bool quit = false;

    Factory factory;
    DestroyHook destroyHook;
    WarmCallback warmCallback;

    std::vector<SlotState> slots;

    /** Slot currently being created on the worker, or -1. */
    int loadingSlot = -1;

    /** High-water mark for `liveCountLocked()`. */
    mutable int peakLiveCount = 0;
    std::vector<int> warmQueue;
    std::vector<std::unique_ptr<Instance>> pendingDestroy;

    int numSlots = 0;
    int neighbourCount = defaultNeighbourCount;
    int activeSlot = -1;

    bool warming = false;
    bool retryDeferred = false;

    double sampleRate = 48000.0;
    int maximumBlockSize = 512;
    bool hasFormat = false;

    std::uint64_t useCounter = 0;
    std::uint64_t warmCount = 0;
    std::uint64_t evictionCount = 0;

    // Shared with the posted delete callbacks, which may outlive the pool.
    std::shared_ptr<std::atomic<int>> pendingDeletes;

    // Declared last: the thread must not start until every other member is
    // initialised.
    std::thread worker;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WarmPool)
};

} // namespace AudioEngine
