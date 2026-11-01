#include "audio/WarmPool.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace AudioEngine
{

namespace
{
constexpr int minimumWarmInstances = 1;
} // namespace

//==============================================================================
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

} // namespace AudioEngine
