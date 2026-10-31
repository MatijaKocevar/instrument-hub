#include "WarmPool.h"

#include <algorithm>
#include <chrono>
#include <limits>

namespace AudioEngine
{

namespace
{
constexpr int minimumWarmInstances = 1;
} // namespace

//==============================================================================
WarmPool::WarmPool (int neighbourCountToUse)
    : neighbourCount (juce::jmax (0, neighbourCountToUse)),
      pendingDeletes (std::make_shared<std::atomic<int>> (0)),
      worker ([this] { run(); })
{
}

WarmPool::~WarmPool()
{
    std::vector<std::unique_ptr<Instance>> leftovers;
    DestroyHook hook;

    {
        const std::lock_guard<std::mutex> lock (mutex);

        quit = true;
        warmQueue.clear();
        hook = destroyHook;

        for (auto& slot : slots)
        {
            if (slot.instance == nullptr)
                continue;

            leftovers.push_back (std::move (slot.instance));
            slot.pins = 0;
        }

        for (auto& instance : pendingDestroy)
            leftovers.push_back (std::move (instance));

        pendingDestroy.clear();
    }

    condition.notify_all();

    if (worker.joinable())
        worker.join();

    // The worker is gone, so these must be handed off here. On the message
    // thread the post runs normally; if the loop has already stopped JUCE's
    // own cleanup path takes over.
    for (auto& instance : leftovers)
        scheduleDestroy (std::move (instance), hook);
}

//==============================================================================
void WarmPool::setFactory (Factory newFactory)
{
    const std::lock_guard<std::mutex> lock (mutex);
    factory = std::move (newFactory);
}

void WarmPool::setDestroyHook (DestroyHook hook)
{
    const std::lock_guard<std::mutex> lock (mutex);
    destroyHook = std::move (hook);
}

void WarmPool::setFormat (double newSampleRate, int newMaximumBlockSize)
{
    const std::lock_guard<std::mutex> lock (mutex);

    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    maximumBlockSize = juce::jmax (1, newMaximumBlockSize);
    hasFormat = true;
}

void WarmPool::setNeighbourCount (int count)
{
    {
        const std::lock_guard<std::mutex> lock (mutex);
        neighbourCount = juce::jmax (0, count);

        // The protected set just changed; let the background thread apply it.
        retryDeferred = true;
        setActiveLocked (activeSlot);
    }

    notifyWorker();
}

int WarmPool::getNeighbourCount() const
{
    const std::lock_guard<std::mutex> lock (mutex);
    return neighbourCount;
}

void WarmPool::setNumSlots (int count)
{
    {
        const std::lock_guard<std::mutex> lock (mutex);

        const int clamped = juce::jmax (0, count);

        if (clamped == numSlots)
            return;

        // Removed slots lose their instances. The caller promised the slots
        // are not processing, so this is a plain move to the destroy queue.
        for (int index = clamped; index < numSlots; ++index)
        {
            auto& slot = slots[static_cast<size_t> (index)];

            if (slot.instance != nullptr)
            {
                pendingDestroy.push_back (std::move (slot.instance));
                ++evictionCount;
            }

            slot.pins = 0;
        }

        slots.resize (static_cast<size_t> (clamped));
        numSlots = clamped;

        warmQueue.clear();

        for (auto& slot : slots)
        {
            slot.queued = false;
            slot.deferred = false;
        }

        if (activeSlot >= numSlots)
            activeSlot = numSlots > 0 ? numSlots - 1 : -1;

        setActiveLocked (activeSlot);
    }

    notifyWorker();
}

int WarmPool::getNumSlots() const
{
    const std::lock_guard<std::mutex> lock (mutex);
    return numSlots;
}

void WarmPool::setRequest (int slotIndex, Request request)
{
    {
        const std::lock_guard<std::mutex> lock (mutex);

        if (! isValidSlotLocked (slotIndex))
            return;

        auto& slot = slots[static_cast<size_t> (slotIndex)];

        const bool sameRequest = slot.request.id == request.id;

        slot.request = std::move (request);
        slot.queued = false;
        slot.deferred = false;
        slot.failedId.clear();
        slot.lastError.clear();

        if (! sameRequest)
            retireLocked (slotIndex, false);

        if (isProtectedLocked (slotIndex))
            enqueueWarmLocked (slotIndex);
    }

    notifyWorker();
}

WarmPool::Request WarmPool::getRequest (int slotIndex) const
{
    const std::lock_guard<std::mutex> lock (mutex);

    if (! isValidSlotLocked (slotIndex))
        return {};

    return slots[static_cast<size_t> (slotIndex)].request;
}

bool WarmPool::hasRequest (int slotIndex) const
{
    const std::lock_guard<std::mutex> lock (mutex);

    if (! isValidSlotLocked (slotIndex))
        return false;

    return slots[static_cast<size_t> (slotIndex)].request.isValid();
}

//==============================================================================
void WarmPool::setActiveSlot (int slotIndex)
{
    {
        const std::lock_guard<std::mutex> lock (mutex);
        setActiveLocked (slotIndex);
    }

    notifyWorker();
}

int WarmPool::getActiveSlot() const
{
    const std::lock_guard<std::mutex> lock (mutex);
    return activeSlot;
}

WarmPool::Instance* WarmPool::armSlot (int slotIndex)
{
    Instance* instance = nullptr;

    {
        const std::lock_guard<std::mutex> lock (mutex);

        if (! isValidSlotLocked (slotIndex))
            return nullptr;

        setActiveLocked (slotIndex);

        auto& slot = slots[static_cast<size_t> (slotIndex)];

        if (slot.instance != nullptr && ! slot.retireAfterRelease)
        {
            ++slot.pins;
            slot.lastUsed = ++useCounter;
            instance = slot.instance.get();
        }
    }

    notifyWorker();
    return instance;
}

WarmPool::Instance* WarmPool::acquire (int slotIndex)
{
    const std::lock_guard<std::mutex> lock (mutex);

    if (! isValidSlotLocked (slotIndex))
        return nullptr;

    auto& slot = slots[static_cast<size_t> (slotIndex)];

    if (slot.instance == nullptr || slot.retireAfterRelease)
        return nullptr;

    ++slot.pins;
    slot.lastUsed = ++useCounter;
    return slot.instance.get();
}

void WarmPool::release (int slotIndex)
{
    {
        const std::lock_guard<std::mutex> lock (mutex);

        if (! isValidSlotLocked (slotIndex))
            return;

        auto& slot = slots[static_cast<size_t> (slotIndex)];

        if (slot.pins > 0)
            --slot.pins;

        if (slot.pins > 0)
            return;

        if (slot.retireAfterRelease)
        {
            retireLocked (slotIndex, true);

            // The replacement request could not be started while the old
            // instance was pinned; start it now.
            if (isProtectedLocked (slotIndex))
                enqueueWarmLocked (slotIndex);
        }
        else if (slot.instance != nullptr && ! isProtectedLocked (slotIndex))
        {
            // The active slot moved on while this one was pinned.
            pendingDestroy.push_back (std::move (slot.instance));
            ++evictionCount;
            slot.lastError.clear();
        }

        // A released pin may have made room for a deferred warm request; the
        // background thread re-queues them before it exits.
        retryDeferred = true;
    }

    notifyWorker();
}

//==============================================================================
WarmPool::PreloadReport WarmPool::preloadAll()
{
    PreloadReport report;

    {
        const std::lock_guard<std::mutex> lock (mutex);

        for (const int index : protectedLocked())
        {
            const auto& slot = slots[static_cast<size_t> (index)];

            if (! slot.request.isValid())
                continue;

            if (slot.instance != nullptr)
            {
                ++report.resident;
                continue;
            }

            if (slot.failedId == slot.request.id)
            {
                ++report.failed;

                if (slot.lastError.isNotEmpty())
                    report.errors.add (slot.lastError);

                continue;
            }

            enqueueWarmLocked (index);

            if (slot.queued || slot.deferred)
                ++report.queued;
        }

        for (int index = 0; index < numSlots; ++index)
        {
            const auto& slot = slots[static_cast<size_t> (index)];

            if (slot.request.isValid() && ! isProtectedLocked (index))
                ++report.deferred;
        }
    }

    notifyWorker();
    return report;
}

//==============================================================================
bool WarmPool::isWarm (int slotIndex) const
{
    const std::lock_guard<std::mutex> lock (mutex);

    if (! isValidSlotLocked (slotIndex))
        return false;

    return slots[static_cast<size_t> (slotIndex)].instance != nullptr;
}

int WarmPool::getInstanceLimit() const
{
    const std::lock_guard<std::mutex> lock (mutex);
    return limitLocked();
}

int WarmPool::getLiveInstanceCount() const
{
    const std::lock_guard<std::mutex> lock (mutex);
    return liveCountLocked();
}

int WarmPool::getPinnedCount() const
{
    const std::lock_guard<std::mutex> lock (mutex);

    int pinned = 0;

    for (const auto& slot : slots)
        if (slot.instance != nullptr && slot.pins > 0)
            ++pinned;

    return pinned;
}

std::uint64_t WarmPool::getWarmCount() const
{
    const std::lock_guard<std::mutex> lock (mutex);
    return warmCount;
}

std::uint64_t WarmPool::getEvictionCount() const
{
    const std::lock_guard<std::mutex> lock (mutex);
    return evictionCount;
}

std::vector<int> WarmPool::getProtectedSlots() const
{
    const std::lock_guard<std::mutex> lock (mutex);
    return protectedLocked();
}

std::vector<WarmPool::SlotInfo> WarmPool::describe() const
{
    const std::lock_guard<std::mutex> lock (mutex);

    std::vector<SlotInfo> result;
    result.reserve (static_cast<size_t> (numSlots));

    for (int index = 0; index < numSlots; ++index)
    {
        const auto& slot = slots[static_cast<size_t> (index)];

        SlotInfo info;
        info.slot = index;
        info.warm = slot.instance != nullptr;
        info.protectedSlot = isProtectedLocked (index);
        info.pins = slot.pins;
        info.id = slot.request.id;
        info.lastError = slot.lastError;

        result.push_back (std::move (info));
    }

    return result;
}

bool WarmPool::isIdle() const
{
    const std::lock_guard<std::mutex> lock (mutex);
    return isIdleLocked();
}

bool WarmPool::waitForIdle (int timeoutMs)
{
    std::unique_lock<std::mutex> lock (mutex);

    return condition.wait_for (lock,
                               std::chrono::milliseconds (juce::jmax (0, timeoutMs)),
                               [this] { return isIdleLocked(); });
}

void WarmPool::clear()
{
    {
        const std::lock_guard<std::mutex> lock (mutex);

        for (int index = 0; index < numSlots; ++index)
            retireLocked (index, true);

        warmQueue.clear();
        retryDeferred = false;

        for (auto& slot : slots)
        {
            slot.queued = false;
            slot.deferred = false;
        }
    }

    notifyWorker();
}

//==============================================================================
std::vector<int> WarmPool::protectedLocked() const
{
    std::vector<int> result;

    if (numSlots <= 0)
        return result;

    const int anchor = isValidSlotLocked (activeSlot) ? activeSlot : 0;

    result.push_back (anchor);

    for (int added = 0; added < neighbourCount; ++added)
    {
        int best = -1;
        int bestDistance = std::numeric_limits<int>::max();

        for (int index = 0; index < numSlots; ++index)
        {
            if (std::find (result.begin(), result.end(), index) != result.end())
                continue;

            // Double the distance and add one for the upper side, so equal
            // distances prefer the lower index deterministically.
            const int distance = std::abs (index - anchor) * 2 + (index > anchor ? 1 : 0);

            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = index;
            }
        }

        if (best < 0)
            break;

        result.push_back (best);
    }

    return result;
}

bool WarmPool::isProtectedLocked (int slotIndex) const
{
    const auto protectedSlots = protectedLocked();

    return std::find (protectedSlots.begin(), protectedSlots.end(), slotIndex)
        != protectedSlots.end();
}

int WarmPool::limitLocked() const
{
    if (numSlots <= 0)
        return 0;

    return juce::jmin (numSlots, juce::jmax (minimumWarmInstances, 1 + neighbourCount));
}

int WarmPool::liveCountLocked() const
{
    // Instances already handed to the message thread for deletion still exist,
    // so they count against the budget until the posted callback has run.
    int live = static_cast<int> (pendingDestroy.size())
             + pendingDeletes->load (std::memory_order_relaxed);

    for (const auto& slot : slots)
        if (slot.instance != nullptr)
            ++live;

    return live;
}

bool WarmPool::isIdleLocked() const
{
    return warmQueue.empty()
        && ! warming
        && pendingDestroy.empty()
        && ! retryDeferred
        && pendingDeletes->load (std::memory_order_relaxed) == 0;
}

bool WarmPool::needsWarmLocked (int slotIndex) const
{
    if (! isValidSlotLocked (slotIndex))
        return false;

    const auto& slot = slots[static_cast<size_t> (slotIndex)];

    if (! slot.request.isValid())
        return false;

    if (slot.instance != nullptr)
        return false;

    if (slot.failedId == slot.request.id)
        return false; // already failed once; wait for an explicit retry

    return isProtectedLocked (slotIndex);
}

void WarmPool::enqueueWarmLocked (int slotIndex)
{
    if (! needsWarmLocked (slotIndex))
        return;

    auto& slot = slots[static_cast<size_t> (slotIndex)];

    if (slot.queued || slot.deferred)
        return;

    if (std::find (warmQueue.begin(), warmQueue.end(), slotIndex) != warmQueue.end())
        return;

    warmQueue.push_back (slotIndex);
    slot.queued = true;
}

bool WarmPool::requeueDeferredLocked()
{
    bool moved = false;

    for (int index = 0; index < numSlots; ++index)
    {
        auto& slot = slots[static_cast<size_t> (index)];

        if (! slot.deferred)
            continue;

        slot.deferred = false;
        enqueueWarmLocked (index);
        moved = true;
    }

    return moved;
}

bool WarmPool::evictOneLocked()
{
    int victim = -1;
    std::uint64_t oldest = std::numeric_limits<std::uint64_t>::max();

    for (int index = 0; index < numSlots; ++index)
    {
        const auto& slot = slots[static_cast<size_t> (index)];

        if (slot.instance == nullptr || slot.pins > 0)
            continue;

        if (isProtectedLocked (index))
            continue;

        if (slot.lastUsed < oldest)
        {
            oldest = slot.lastUsed;
            victim = index;
        }
    }

    if (victim < 0)
        return false;

    auto& slot = slots[static_cast<size_t> (victim)];

    pendingDestroy.push_back (std::move (slot.instance));
    slot.lastError.clear();
    ++evictionCount;

    return true;
}

void WarmPool::setActiveLocked (int slotIndex)
{
    if (numSlots <= 0)
    {
        activeSlot = -1;
        return;
    }

    activeSlot = isValidSlotLocked (slotIndex) ? slotIndex : -1;

    // Evict everything outside the new neighbourhood right away; this is what
    // bounds memory when a pad arms a distant slot.
    for (int index = 0; index < numSlots; ++index)
    {
        auto& slot = slots[static_cast<size_t> (index)];

        if (slot.instance == nullptr || slot.pins > 0)
            continue;

        if (isProtectedLocked (index))
            continue;

        pendingDestroy.push_back (std::move (slot.instance));
        slot.lastError.clear();
        ++evictionCount;
    }

    for (const int index : protectedLocked())
        enqueueWarmLocked (index);
}

void WarmPool::retireLocked (int slotIndex, bool force)
{
    if (! isValidSlotLocked (slotIndex))
        return;

    auto& slot = slots[static_cast<size_t> (slotIndex)];

    if (slot.instance == nullptr)
        return;

    if (slot.pins > 0 && ! force)
    {
        // Still in use. `release()` retires it as soon as the pin drops.
        slot.retireAfterRelease = true;
        return;
    }

    pendingDestroy.push_back (std::move (slot.instance));
    slot.pins = 0;
    slot.retireAfterRelease = false;
    slot.lastError.clear();
    ++evictionCount;
}

//==============================================================================
void WarmPool::scheduleDestroy (std::unique_ptr<Instance> instance, const DestroyHook& hook)
{
    if (instance == nullptr)
        return;

    pendingDeletes->fetch_add (1, std::memory_order_relaxed);

    auto counter = pendingDeletes;
    std::shared_ptr<Instance> shared (std::move (instance));

    const bool posted = juce::MessageManager::callAsync ([shared, hook, counter] () mutable
    {
        if (hook != nullptr)
        {
            try
            {
                hook (*shared);
            }
            catch (...)
            {
            }
        }

        // The plugin's destructor runs here, on the message thread, which is
        // where JUCE's VST3 wrapper expects it (`callSync` in its cleanup).
        shared.reset();
        counter->fetch_sub (1, std::memory_order_relaxed);
    });

    if (! posted)
    {
        // The message loop is already gone (application shutdown). There is no
        // safe thread left to defer to, so finish here.
        if (hook != nullptr)
        {
            try
            {
                hook (*shared);
            }
            catch (...)
            {
            }
        }

        shared.reset();
        counter->fetch_sub (1, std::memory_order_relaxed);
    }
}

std::unique_ptr<WarmPool::Instance> WarmPool::createInstance (const Request& request,
                                                              juce::String& error)
{
    Factory currentFactory;
    bool shouldPrepare = false;
    double rate = 0.0;
    int blockSize = 0;

    {
        const std::lock_guard<std::mutex> lock (mutex);
        currentFactory = factory;
        shouldPrepare = hasFormat;
        rate = sampleRate;
        blockSize = maximumBlockSize;
    }

    if (currentFactory == nullptr)
    {
        error = "no factory installed";
        return nullptr;
    }

    std::unique_ptr<Instance> created;

    try
    {
        created = currentFactory (request, error);
    }
    catch (const std::exception& exception)
    {
        error = juce::String ("factory threw: ") + exception.what();
        created.reset();
    }
    catch (...)
    {
        error = "factory threw an unknown exception";
        created.reset();
    }

    if (created != nullptr && shouldPrepare)
        created->prepare (rate, blockSize);

    return created;
}

void WarmPool::run()
{
    std::unique_lock<std::mutex> lock (mutex);

    for (;;)
    {
        if (quit && pendingDestroy.empty())
            break;

        // Retired instances are handed to the message thread before a new load,
        // so the live-instance count never exceeds the limit while a
        // replacement is created.
        if (! pendingDestroy.empty())
        {
            auto batch = std::move (pendingDestroy);
            pendingDestroy.clear();
            const auto hook = destroyHook;

            lock.unlock();

            for (auto& instance : batch)
                scheduleDestroy (std::move (instance), hook);

            batch.clear();

            lock.lock();

            condition.notify_all();
            continue;
        }

        if (quit)
            break;

        if (retryDeferred)
        {
            retryDeferred = false;
            requeueDeferredLocked();

            condition.notify_all();
            continue;
        }

        if (warmQueue.empty())
        {
            // A slot deferred earlier becomes reachable again as soon as a pin
            // is dropped or a delete completes.
            if ((liveCountLocked() < limitLocked()
                 || pendingDeletes->load (std::memory_order_relaxed) > 0)
                && requeueDeferredLocked())
            {
                condition.notify_all();
                continue;
            }

            condition.notify_all();
            condition.wait (lock);
            continue;
        }

        const int slotIndex = warmQueue.front();
        warmQueue.erase (warmQueue.begin());
        slots[static_cast<size_t> (slotIndex)].queued = false;

        if (! needsWarmLocked (slotIndex))
        {
            condition.notify_all();
            continue;
        }

        if (liveCountLocked() >= limitLocked())
        {
            if (evictOneLocked())
            {
                // Re-queue and let the retire pass run first, so the count
                // really drops before this load starts.
                warmQueue.insert (warmQueue.begin(), slotIndex);
                slots[static_cast<size_t> (slotIndex)].queued = true;
                continue;
            }

            if (pendingDeletes->load (std::memory_order_relaxed) > 0)
            {
                // Room is on its way: the message thread just has to run the
                // posted deletes. Poll briefly rather than spin.
                warmQueue.insert (warmQueue.begin(), slotIndex);
                slots[static_cast<size_t> (slotIndex)].queued = true;
                condition.wait_for (lock, std::chrono::milliseconds (2));
                continue;
            }

            // Every other instance is pinned; try again after a release().
            slots[static_cast<size_t> (slotIndex)].deferred = true;
            condition.notify_all();
            continue;
        }

        const Request request = slots[static_cast<size_t> (slotIndex)].request;

        warming = true;
        condition.notify_all();
        lock.unlock();

        juce::String error;
        auto created = createInstance (request, error);

        lock.lock();
        warming = false;

        auto& slot = slots[static_cast<size_t> (slotIndex)];
        const bool stillNeeded = needsWarmLocked (slotIndex);

        if (created != nullptr && stillNeeded && liveCountLocked() < limitLocked())
        {
            slot.instance = std::move (created);
            slot.lastUsed = ++useCounter;
            slot.lastError.clear();
            slot.failedId.clear();
            ++warmCount;
        }
        else
        {
            if (created != nullptr)
                pendingDestroy.push_back (std::move (created));

            if (error.isNotEmpty())
            {
                slot.lastError = error;
                slot.failedId = request.id;
            }
            else if (stillNeeded && isProtectedLocked (slotIndex))
            {
                // A message-thread eviction filled the budget while this load
                // was in flight. Try again once it has been freed.
                slot.deferred = true;
            }
        }

        condition.notify_all();
    }

    condition.notify_all();
}

void WarmPool::notifyWorker()
{
    condition.notify_all();
}

} // namespace AudioEngine
