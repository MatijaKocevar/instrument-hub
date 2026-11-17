#include "audio/WarmPool.h"

#include <utility>

namespace AudioEngine {

void WarmPool::setNeighbourCount(int count) {
    {
        const std::lock_guard<std::mutex> lock(mutex);
        neighbourCount = juce::jmax(0, count);

        retryDeferred = true;
        setActiveLocked(activeSlot);
    }

    notifyWorker();
}

int WarmPool::getNeighbourCount() const {
    const std::lock_guard<std::mutex> lock(mutex);
    return neighbourCount;
}

void WarmPool::setNumSlots(int count) {
    {
        const std::lock_guard<std::mutex> lock(mutex);

        const int clamped = juce::jmax(0, count);

        if (clamped == numSlots) return;

        for (int index = clamped; index < numSlots; ++index) {
            auto& slot = slots[static_cast<size_t>(index)];

            if (slot.instance != nullptr) {
                pendingDestroy.push_back(std::move(slot.instance));
                ++evictionCount;
            }

            slot.pins = 0;
        }

        slots.resize(static_cast<size_t>(clamped));
        numSlots = clamped;

        warmQueue.clear();

        for (auto& slot : slots) {
            slot.queued = false;
            slot.deferred = false;
        }

        if (activeSlot >= numSlots) activeSlot = numSlots > 0 ? numSlots - 1 : -1;

        setActiveLocked(activeSlot);
    }

    notifyWorker();
}

int WarmPool::getNumSlots() const {
    const std::lock_guard<std::mutex> lock(mutex);
    return numSlots;
}

void WarmPool::setRequest(int slotIndex, Request request) {
    {
        const std::lock_guard<std::mutex> lock(mutex);

        if (!isValidSlotLocked(slotIndex)) return;

        auto& slot = slots[static_cast<size_t>(slotIndex)];

        const bool sameRequest = slot.request.id == request.id;

        slot.request = std::move(request);
        slot.queued = false;
        slot.deferred = false;
        slot.failedId.clear();
        slot.lastError.clear();

        if (!sameRequest) retireLocked(slotIndex, false);

        if (isRetainedLocked(slotIndex)) enqueueWarmLocked(slotIndex);
    }

    notifyWorker();
}

WarmPool::Request WarmPool::getRequest(int slotIndex) const {
    const std::lock_guard<std::mutex> lock(mutex);

    if (!isValidSlotLocked(slotIndex)) return {};

    return slots[static_cast<size_t>(slotIndex)].request;
}

bool WarmPool::hasRequest(int slotIndex) const {
    const std::lock_guard<std::mutex> lock(mutex);

    if (!isValidSlotLocked(slotIndex)) return false;

    return slots[static_cast<size_t>(slotIndex)].request.isValid();
}

bool WarmPool::needsWarmLocked(int slotIndex) const {
    if (!isValidSlotLocked(slotIndex)) return false;

    const auto& slot = slots[static_cast<size_t>(slotIndex)];

    if (!slot.request.isValid()) return false;

    if (slot.instance != nullptr) return false;

    if (slot.failedId == slot.request.id) return false;

    return isRetainedLocked(slotIndex);
}

}  // namespace AudioEngine
