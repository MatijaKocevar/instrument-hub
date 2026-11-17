#include "WarmPool.h"

#include <chrono>
#include <utility>

namespace AudioEngine {

WarmPool::WarmPool(int neighbourCountToUse)
    : neighbourCount(juce::jmax(0, neighbourCountToUse)),
      pendingDeletes(std::make_shared<std::atomic<int>>(0)),
      worker([this] { run(); }) {}

void WarmPool::setFactory(Factory newFactory) {
    const std::lock_guard<std::mutex> lock(mutex);
    factory = std::move(newFactory);
}

void WarmPool::setDestroyHook(DestroyHook hook) {
    const std::lock_guard<std::mutex> lock(mutex);
    destroyHook = std::move(hook);
}

void WarmPool::setWarmCallback(WarmCallback callback) {
    const std::lock_guard<std::mutex> lock(mutex);
    warmCallback = std::move(callback);
}

void WarmPool::setFormat(double newSampleRate, int newMaximumBlockSize) {
    const std::lock_guard<std::mutex> lock(mutex);

    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    maximumBlockSize = juce::jmax(1, newMaximumBlockSize);
    hasFormat = true;

    for (auto& slot : slots)
        if (slot.instance != nullptr) slot.instance->prepare(sampleRate, maximumBlockSize);
}

int WarmPool::getInstanceLimit() const {
    const std::lock_guard<std::mutex> lock(mutex);
    return limitLocked();
}

std::uint64_t WarmPool::getWarmCount() const {
    const std::lock_guard<std::mutex> lock(mutex);
    return warmCount;
}

std::vector<int> WarmPool::getProtectedSlots() const {
    const std::lock_guard<std::mutex> lock(mutex);
    return protectedLocked();
}

int WarmPool::getProtectedCount() const {
    const std::lock_guard<std::mutex> lock(mutex);
    return static_cast<int>(protectedLocked().size());
}

std::vector<WarmPool::SlotInfo> WarmPool::describe() const {
    const std::lock_guard<std::mutex> lock(mutex);

    std::vector<SlotInfo> result;
    result.reserve(static_cast<size_t>(numSlots));

    for (int index = 0; index < numSlots; ++index) {
        const auto& slot = slots[static_cast<size_t>(index)];

        SlotInfo info;
        info.slot = index;
        info.warm = slot.instance != nullptr;
        info.protectedSlot = isProtectedLocked(index);
        info.pins = slot.pins;
        info.id = slot.request.id;
        info.lastError = slot.lastError;

        result.push_back(std::move(info));
    }

    return result;
}

bool WarmPool::waitForIdle(int timeoutMs) {
    std::unique_lock<std::mutex> lock(mutex);

    return condition.wait_for(lock, std::chrono::milliseconds(juce::jmax(0, timeoutMs)),
                              [this] { return isIdleLocked(); });
}

}  // namespace AudioEngine
