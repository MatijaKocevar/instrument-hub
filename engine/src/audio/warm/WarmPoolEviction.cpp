#include "audio/WarmPool.h"

#include <limits>
#include <utility>

namespace AudioEngine
{

    int WarmPool::getLiveInstanceCount () const
    {
        const std::lock_guard<std::mutex> lock (mutex);
        return liveCountLocked ();
    }

    int WarmPool::getPeakLiveInstanceCount () const
    {
        const std::lock_guard<std::mutex> lock (mutex);
        (void)liveCountLocked ();
        return peakLiveCount;
    }

    int WarmPool::getPinnedCount () const
    {
        const std::lock_guard<std::mutex> lock (mutex);

        int pinned = 0;

        for (const auto& slot : slots)
            if (slot.instance != nullptr && slot.pins > 0)
                ++pinned;

        return pinned;
    }

    std::uint64_t WarmPool::getEvictionCount () const
    {
        const std::lock_guard<std::mutex> lock (mutex);
        return evictionCount;
    }

    bool WarmPool::isIdle () const
    {
        const std::lock_guard<std::mutex> lock (mutex);
        return isIdleLocked ();
    }

    void WarmPool::clear ()
    {
        {
            const std::lock_guard<std::mutex> lock (mutex);

            for (int index = 0; index < numSlots; ++index)
                retireLocked (index, true);

            warmQueue.clear ();
            retryDeferred = false;

            for (auto& slot : slots)
            {
                slot.queued = false;
                slot.deferred = false;
            }
        }

        notifyWorker ();
    }

    int WarmPool::liveCountLocked () const
    {
        int live = static_cast<int> (pendingDestroy.size ()) +
                   pendingDeletes->load (std::memory_order_relaxed);

        for (const auto& slot : slots)
            if (slot.instance != nullptr)
                ++live;

        peakLiveCount = juce::jmax (peakLiveCount, live);
        return live;
    }

    bool WarmPool::isIdleLocked () const
    {
        return warmQueue.empty () && !warming && pendingDestroy.empty () && !retryDeferred &&
               pendingDeletes->load (std::memory_order_relaxed) == 0;
    }

    bool WarmPool::evictOneLocked ()
    {
        int victim = -1;
        std::uint64_t oldest = std::numeric_limits<std::uint64_t>::max ();

        for (int index = 0; index < numSlots; ++index)
        {
            const auto& slot = slots[static_cast<size_t> (index)];

            if (slot.instance == nullptr || slot.pins > 0)
                continue;

            if (isRetainedLocked (index))
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
        slot.lastError.clear ();
        ++evictionCount;

        return true;
    }

    void WarmPool::retireLocked (int slotIndex, bool force)
    {
        if (!isValidSlotLocked (slotIndex))
            return;

        auto& slot = slots[static_cast<size_t> (slotIndex)];

        if (slot.instance == nullptr)
            return;

        if (slot.pins > 0 && !force)
        {
            slot.retireAfterRelease = true;
            return;
        }

        pendingDestroy.push_back (std::move (slot.instance));
        slot.pins = 0;
        slot.retireAfterRelease = false;
        slot.lastError.clear ();
        ++evictionCount;
    }

    void WarmPool::scheduleDestroy (std::unique_ptr<Instance> instance, const DestroyHook& hook)
    {
        if (instance == nullptr)
            return;

        pendingDeletes->fetch_add (1, std::memory_order_relaxed);

        auto counter = pendingDeletes;
        std::shared_ptr<Instance> shared (std::move (instance));

        const bool posted = juce::MessageManager::callAsync (
            [shared, hook, counter] () mutable
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

                shared.reset ();
                counter->fetch_sub (1, std::memory_order_relaxed);
            });

        if (!posted)
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

            shared.reset ();
            counter->fetch_sub (1, std::memory_order_relaxed);
        }
    }

}
