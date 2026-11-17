#include "TapTempo.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace AudioEngine {

namespace {
constexpr double rangeEpsilon = 1.0e-9;
}

TapTempo::TapTempo(int requestedHistorySize) noexcept
    : historySize(std::clamp(requestedHistorySize, 1, maximumHistorySize)) {}

double TapTempo::tapAt(double timestampSeconds) noexcept {
    if (!std::isfinite(timestampSeconds)) return getBpm();

    double estimate = 0.0;

    {
        const ScopedLock lock(busy);

        bool startNewSequence = (taps == 0);

        if (!startNewSequence) {
            const double interval = timestampSeconds - lastTapTime;

            if (!(interval > 0.0) || interval + rangeEpsilon < minimumIntervalSeconds ||
                interval > maximumIntervalSeconds + rangeEpsilon) {
                startNewSequence = true;
            } else {
                ++taps;
                lastTapTime = timestampSeconds;

                intervals[intervalHead] = interval;
                intervalHead = (intervalHead + 1) % historySize;

                if (intervalCount < historySize) ++intervalCount;

                const double median = medianIntervalSeconds();

                if (median > 0.0)
                    estimate = std::clamp(bpmForIntervalSeconds(median), minBpm, maxBpm);

                lastIntervalSeconds.store(interval, std::memory_order_relaxed);
            }
        }

        if (startNewSequence) {
            taps = 1;
            lastTapTime = timestampSeconds;
            intervalCount = 0;
            intervalHead = 0;
            estimate = 0.0;

            lastIntervalSeconds.store(0.0, std::memory_order_relaxed);
        }

        bpm.store(estimate, std::memory_order_relaxed);
        tapCount.store(taps, std::memory_order_relaxed);
        publishedIntervalCount.store(intervalCount, std::memory_order_relaxed);
    }

    return estimate;
}

double TapTempo::tap() noexcept {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();

    return tapAt(std::chrono::duration<double>(now).count());
}

void TapTempo::reset() noexcept {
    {
        const ScopedLock lock(busy);

        taps = 0;
        lastTapTime = 0.0;
        intervalCount = 0;
        intervalHead = 0;

        for (auto& interval : intervals) interval = 0.0;
    }

    bpm.store(0.0, std::memory_order_relaxed);
    tapCount.store(0, std::memory_order_relaxed);
    publishedIntervalCount.store(0, std::memory_order_relaxed);
    lastIntervalSeconds.store(0.0, std::memory_order_relaxed);
}

double TapTempo::getBpm() const noexcept { return bpm.load(std::memory_order_relaxed); }

bool TapTempo::hasEstimate() const noexcept { return getBpm() > 0.0; }

int TapTempo::getTapCount() const noexcept { return tapCount.load(std::memory_order_relaxed); }

int TapTempo::getIntervalCount() const noexcept {
    return publishedIntervalCount.load(std::memory_order_relaxed);
}

int TapTempo::getHistorySize() const noexcept { return historySize; }

double TapTempo::getLastIntervalSeconds() const noexcept {
    return lastIntervalSeconds.load(std::memory_order_relaxed);
}

double TapTempo::medianIntervalSeconds() const noexcept {
    if (intervalCount <= 0) return 0.0;

    std::array<double, maximumHistorySize> sorted{};

    for (int index = 0; index < intervalCount; ++index)
        sorted[static_cast<std::size_t>(index)] = intervals[index];

    std::sort(sorted.begin(), sorted.begin() + intervalCount);

    if (intervalCount % 2 == 1) return sorted[static_cast<std::size_t>(intervalCount / 2)];

    const auto upper = intervalCount / 2;

    return 0.5 *
           (sorted[static_cast<std::size_t>(upper - 1)] + sorted[static_cast<std::size_t>(upper)]);
}

}  // namespace AudioEngine
