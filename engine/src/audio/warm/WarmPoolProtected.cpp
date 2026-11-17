#include "audio/WarmPool.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace AudioEngine {

namespace {
constexpr int minimumWarmInstances = 1;
}

void WarmPool::setProtectedSlots(std::vector<int> newSlots) {
    {
        const std::lock_guard<std::recursive_mutex> lock(mutex);

        explicitProtectedSlots = std::move(newSlots);
        retryDeferred = true;
        setActiveLocked(activeSlot);
    }

    notifyWorker();
}

std::vector<int> WarmPool::protectedLocked() const {
    std::vector<int> result;

    if (numSlots <= 0) return result;

    if (!explicitProtectedSlots.empty()) {
        for (const int index : explicitProtectedSlots) {
            if (!isValidSlotLocked(index)) continue;

            if (std::find(result.begin(), result.end(), index) != result.end()) continue;

            result.push_back(index);
        }

        return result;
    }

    const int anchor = isValidSlotLocked(activeSlot) ? activeSlot : 0;

    result.push_back(anchor);

    for (int added = 0; added < neighbourCount; ++added) {
        int best = -1;
        int bestDistance = std::numeric_limits<int>::max();

        for (int index = 0; index < numSlots; ++index) {
            if (std::find(result.begin(), result.end(), index) != result.end()) continue;

            const int distance = std::abs(index - anchor) * 2 + (index > anchor ? 1 : 0);

            if (distance < bestDistance) {
                bestDistance = distance;
                best = index;
            }
        }

        if (best < 0) break;

        result.push_back(best);
    }

    return result;
}

std::vector<int> WarmPool::retainedLocked() const {
    auto result = protectedLocked();

    if (isValidSlotLocked(activeSlot) &&
        std::find(result.begin(), result.end(), activeSlot) == result.end()) {
        result.push_back(activeSlot);
    }

    return result;
}

bool WarmPool::isProtectedLocked(int slotIndex) const {
    const auto protectedSlots = protectedLocked();

    return std::find(protectedSlots.begin(), protectedSlots.end(), slotIndex) !=
           protectedSlots.end();
}

bool WarmPool::isRetainedLocked(int slotIndex) const {
    const auto retainedSlots = retainedLocked();

    return std::find(retainedSlots.begin(), retainedSlots.end(), slotIndex) != retainedSlots.end();
}

int WarmPool::limitLocked() const {
    if (numSlots <= 0) return 0;

    if (!explicitProtectedSlots.empty()) {
        const int desired =
            juce::jmax(minimumWarmInstances, static_cast<int>(retainedLocked().size()));

        return juce::jmin(numSlots, maximumWarmInstances, desired);
    }

    return juce::jmin(numSlots, juce::jmax(minimumWarmInstances, 1 + neighbourCount));
}

}  // namespace AudioEngine
