#include "audio/WarmPool.h"

#include <algorithm>
#include <utility>

namespace AudioEngine
{

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

            if (isProtectedLocked (slotIndex))
                enqueueWarmLocked (slotIndex);
        }
        else if (slot.instance != nullptr && ! isProtectedLocked (slotIndex))
        {
            pendingDestroy.push_back (std::move (slot.instance));
            ++evictionCount;
            slot.lastError.clear();
        }

        retryDeferred = true;
    }

    notifyWorker();
}

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

bool WarmPool::isWarm (int slotIndex) const
{
    const std::lock_guard<std::mutex> lock (mutex);

    if (! isValidSlotLocked (slotIndex))
        return false;

    return slots[static_cast<size_t> (slotIndex)].instance != nullptr;
}

bool WarmPool::isWarming (int slotIndex) const
{
    const std::lock_guard<std::mutex> lock (mutex);

    if (! isValidSlotLocked (slotIndex))
        return false;

    if (loadingSlot == slotIndex)
        return true;

    const auto& slot = slots[static_cast<size_t> (slotIndex)];
    return slot.queued || slot.deferred;
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

void WarmPool::setActiveLocked (int slotIndex)
{
    if (numSlots <= 0)
    {
        activeSlot = -1;
        return;
    }

    activeSlot = isValidSlotLocked (slotIndex) ? slotIndex : -1;

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

}
