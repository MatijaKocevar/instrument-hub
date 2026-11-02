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

class WarmPool
{
public:
    using Instance = SlotSource;

    struct Request
    {
        juce::String id;
        std::any payload;

        bool isValid() const noexcept { return id.isNotEmpty(); }
    };

    struct PreloadReport
    {
        int resident = 0;
        int queued = 0;
        int deferred = 0;
        int failed = 0;
        juce::StringArray errors;

        bool isPending() const noexcept { return queued > 0; }
        bool isComplete() const noexcept { return ! isPending() && failed == 0; }
    };

    struct SlotInfo
    {
        int slot = -1;
        bool warm = false;
        bool protectedSlot = false;
        int pins = 0;
        juce::String id;
        juce::String lastError;
    };

    using Factory = std::function<std::unique_ptr<Instance> (const Request&, juce::String& error)>;

    using DestroyHook = std::function<void (Instance&)>;

    using WarmCallback = std::function<void (int slot)>;

    static constexpr int defaultNeighbourCount = 2;

    explicit WarmPool (int neighbourCount = defaultNeighbourCount);

    ~WarmPool();

    void setFactory (Factory factory);
    void setDestroyHook (DestroyHook hook);
    void setWarmCallback (WarmCallback callback);

    void setFormat (double sampleRate, int maximumBlockSize);

    void setNeighbourCount (int count);
    int getNeighbourCount() const;

    void setNumSlots (int count);
    int getNumSlots() const;

    void setRequest (int slot, Request request);

    Request getRequest (int slot) const;
    bool hasRequest (int slot) const;

    void setActiveSlot (int slot);
    int getActiveSlot() const;

    Instance* armSlot (int slot);

    Instance* acquire (int slot);

    void release (int slot);

    PreloadReport preloadAll();

    bool isWarm (int slot) const;

    bool isWarming (int slot) const;

    int getInstanceLimit() const;

    int getLiveInstanceCount() const;

    int getPeakLiveInstanceCount() const;

    int getPinnedCount() const;

    std::uint64_t getWarmCount() const;

    std::uint64_t getEvictionCount() const;

    std::vector<int> getProtectedSlots() const;

    std::vector<SlotInfo> describe() const;

    bool isIdle() const;

    bool waitForIdle (int timeoutMs = 10000);

    void clear();

    bool shutdown (int timeoutMs = 2000) noexcept;

private:
    struct SlotState
    {
        Request request;
        std::unique_ptr<Instance> instance;

        juce::String lastError;
        juce::String failedId;

        int pins = 0;
        std::uint64_t lastUsed = 0;

        bool retireAfterRelease = false;
        bool queued = false;
        bool deferred = false;
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

    void scheduleDestroy (std::unique_ptr<Instance> instance, const DestroyHook& hook);

    std::unique_ptr<Instance> createInstance (const Request& request, juce::String& error);
    void run();

    void notifyWorker();

    mutable std::mutex mutex;
    std::condition_variable condition;
    std::condition_variable workerDoneCondition;

    bool quit = false;
    bool workerDone = false;
    bool shutdownComplete = false;

    Factory factory;
    DestroyHook destroyHook;
    WarmCallback warmCallback;

    std::vector<SlotState> slots;

    int loadingSlot = -1;

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

    std::shared_ptr<std::atomic<int>> pendingDeletes;

    std::thread worker;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WarmPool)
};

}
