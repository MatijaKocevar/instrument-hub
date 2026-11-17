#include "ClockIn.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>

namespace AudioEngine {

namespace {
constexpr double intervalTolerance = 1.0e-9;

constexpr double maxPhaseFraction = 0.999999;
}  // namespace

void ClockIn::reset() noexcept {
    clearQueue();

    estimateBpm = 0.0;
    intervalCount = 0;
    intervalHead = 0;

    for (auto& interval : intervalHistory) interval = 0.0;

    haveTimestamp = false;
    lastTimestamp = 0.0;

    seenClock = false;
    playingInternal = false;
    lockedInternal = false;

    positionTicks = 0;
    clockCountInternal = 0;
    samplesSinceTick = 0.0;
    sinceClockSeconds = 0.0;

    locked.store(false, std::memory_order_relaxed);
    playing.store(false, std::memory_order_relaxed);
    clockPresent.store(false, std::memory_order_relaxed);
    bpm.store(0.0, std::memory_order_relaxed);
    ppq.store(0.0, std::memory_order_relaxed);
    tickCount.store(0, std::memory_order_relaxed);
    clockCount.store(0, std::memory_order_relaxed);
    secondsSinceClock.store(0.0, std::memory_order_relaxed);
    droppedEvents.store(0, std::memory_order_relaxed);
}

int ClockIn::clearQueue() noexcept {
    const int read = queueReadIndex.load(std::memory_order_relaxed);
    const int write = queueWriteIndex.load(std::memory_order_acquire);
    const int pending = (write + queueCapacity - read) % queueCapacity;

    queueReadIndex.store(write, std::memory_order_release);
    return pending;
}

bool ClockIn::pushRealtime(std::uint8_t statusByte, double timestampSeconds) noexcept {
    Type type;

    switch (statusByte) {
        case static_cast<std::uint8_t>(Type::clock):
            type = Type::clock;
            break;
        case static_cast<std::uint8_t>(Type::start):
            type = Type::start;
            break;
        case static_cast<std::uint8_t>(Type::continue_):
            type = Type::continue_;
            break;
        case static_cast<std::uint8_t>(Type::stop):
            type = Type::stop;
            break;
        default:
            return false;
    }

    Event event;
    event.type = type;
    event.timestampSeconds = std::isfinite(timestampSeconds) ? timestampSeconds : 0.0;

    return push(event);
}

void ClockIn::process(Transport& transport, int numSamples) noexcept {
    const double sampleRate = transport.getSampleRate();

    if (!(sampleRate > 0.0) || numSamples <= 0) return;

    Event events[64];
    bool tickedThisBlock = false;

    for (;;) {
        const int count = drain(events, static_cast<int>(std::size(events)));

        for (int index = 0; index < count; ++index) {
            const auto& event = events[index];

            if (event.type != Type::clock) {
                handleEvent(event);
                continue;
            }

            seenClock = true;
            clockPresent.store(true, std::memory_order_relaxed);
            sinceClockSeconds = 0.0;

            if (playingInternal) {
                ++positionTicks;
                samplesSinceTick = 0.0;
                tickedThisBlock = true;
            }

            ++clockCountInternal;

            if (haveTimestamp) {
                const double interval = event.timestampSeconds - lastTimestamp;
                const double minimum = tickPeriodSeconds(Transport::maxBpm);
                const double maximum = tickPeriodSeconds(Transport::minBpm);

                if (interval >= minimum - intervalTolerance &&
                    interval <= maximum + intervalTolerance) {
                    addInterval(interval);
                } else {
                    const auto ticks = clockCountInternal;
                    resetTempoRun();
                    clockCountInternal = ticks;
                }
            }

            haveTimestamp = true;
            lastTimestamp = event.timestampSeconds;
        }

        if (count < static_cast<int>(std::size(events))) break;
    }

    sinceClockSeconds += static_cast<double>(numSamples) / sampleRate;

    if (seenClock) {
        const double tempo = estimateBpm > 0.0 ? estimateBpm : Transport::defaultBpm;
        const double timeout = std::max(0.5, 6.0 * tickPeriodSeconds(tempo));

        if (sinceClockSeconds > timeout) loseClock();
    }

    if (intervalCount >= minTempoIntervals) {
        const double median = medianInterval();

        if (median > 0.0) {
            const double measured = clampBpm(60.0 / (median * pulsesPerQuarterNote));

            if (!lockedInternal || estimateBpm <= 0.0)
                estimateBpm = measured;
            else
                estimateBpm += tempoSmoothing * (measured - estimateBpm);
        }
    }

    if (!lockedInternal && intervalCount >= minTempoIntervals && clockCountInternal >= lockTicks) {
        lockedInternal = true;
    }

    double ppqNow = static_cast<double>(positionTicks) / pulsesPerQuarterNote;

    if (estimateBpm > 0.0) {
        const double quartersPerSample = estimateBpm / (60.0 * sampleRate);
        double phase = samplesSinceTick * quartersPerSample;

        phase = std::clamp(phase, 0.0, maxPhaseFraction / pulsesPerQuarterNote);
        ppqNow += phase;
    }

    locked.store(lockedInternal, std::memory_order_relaxed);
    playing.store(playingInternal && clockPresent.load(std::memory_order_relaxed),
                  std::memory_order_relaxed);
    bpm.store(estimateBpm, std::memory_order_relaxed);
    ppq.store(ppqNow, std::memory_order_relaxed);
    tickCount.store(positionTicks, std::memory_order_relaxed);
    clockCount.store(clockCountInternal, std::memory_order_relaxed);
    secondsSinceClock.store(sinceClockSeconds, std::memory_order_relaxed);

    if (!seenClock) return;

    const bool present = clockPresent.load(std::memory_order_relaxed);
    const double appliedBpm = estimateBpm > 0.0 ? estimateBpm : transport.getBpm();

    Transport::ExternalSync sync;
    sync.locked = present;
    sync.playing = present && playingInternal;
    sync.bpm = appliedBpm;
    sync.ppq = ppqNow;

    const double frames = appliedBpm > 0.0 ? ppqNow * 60.0 * sampleRate / appliedBpm : 0.0;

    sync.positionSamples =
        std::isfinite(frames) && frames > 0.0 ? static_cast<std::int64_t>(std::llround(frames)) : 0;

    transport.applyExternalSync(sync);

    if (playingInternal && !tickedThisBlock) samplesSinceTick += static_cast<double>(numSamples);
}

void ClockIn::handleEvent(const Event& event) noexcept {
    seenClock = true;
    clockPresent.store(true, std::memory_order_relaxed);
    sinceClockSeconds = 0.0;

    switch (event.type) {
        case Type::clock:
            break;

        case Type::start:
            playingInternal = true;
            positionTicks = 0;
            samplesSinceTick = 0.0;
            resetTempoRun();
            break;

        case Type::continue_:
            playingInternal = true;
            resetTempoRun();
            break;

        case Type::stop:
            playingInternal = false;
            break;
    }
}

void ClockIn::resetTempoRun() noexcept {
    intervalCount = 0;
    intervalHead = 0;
    haveTimestamp = false;
    lockedInternal = false;
    clockCountInternal = 0;
}

void ClockIn::addInterval(double seconds) noexcept {
    intervalHistory[intervalHead] = seconds;
    intervalHead = (intervalHead + 1) % tempoHistorySize;

    if (intervalCount < tempoHistorySize) ++intervalCount;
}

double ClockIn::medianInterval() const noexcept {
    if (intervalCount <= 0) return 0.0;

    double sorted[tempoHistorySize]{};

    for (int index = 0; index < intervalCount; ++index) sorted[index] = intervalHistory[index];

    std::sort(sorted, sorted + intervalCount);

    return intervalCount % 2 == 1
               ? sorted[intervalCount / 2]
               : 0.5 * (sorted[intervalCount / 2 - 1] + sorted[intervalCount / 2]);
}

void ClockIn::loseClock() noexcept {
    clockPresent.store(false, std::memory_order_relaxed);
    playingInternal = false;
    resetTempoRun();
    samplesSinceTick = 0.0;
}

int ClockIn::drain(Event* destination, int capacity) noexcept {
    if (destination == nullptr || capacity <= 0) return 0;

    int read = queueReadIndex.load(std::memory_order_relaxed);
    const int write = queueWriteIndex.load(std::memory_order_acquire);
    int count = 0;

    while (read != write && count < capacity) {
        destination[count++] = queue[read];
        read = (read + 1) % queueCapacity;
    }

    queueReadIndex.store(read, std::memory_order_release);
    return count;
}

bool ClockIn::push(const Event& event) noexcept {
    const int write = queueWriteIndex.load(std::memory_order_relaxed);
    const int next = (write + 1) % queueCapacity;

    if (next == queueReadIndex.load(std::memory_order_acquire)) {
        droppedEvents.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    queue[write] = event;

    queueWriteIndex.store(next, std::memory_order_release);
    return true;
}

bool ClockIn::isLocked() const noexcept { return locked.load(std::memory_order_relaxed); }

bool ClockIn::isPlaying() const noexcept { return playing.load(std::memory_order_relaxed); }

bool ClockIn::isClockPresent() const noexcept {
    return clockPresent.load(std::memory_order_relaxed);
}

double ClockIn::getBpm() const noexcept { return bpm.load(std::memory_order_relaxed); }

double ClockIn::getPpq() const noexcept { return ppq.load(std::memory_order_relaxed); }

std::int64_t ClockIn::getTickCount() const noexcept {
    return tickCount.load(std::memory_order_relaxed);
}

std::int64_t ClockIn::getClockCount() const noexcept {
    return clockCount.load(std::memory_order_relaxed);
}

int ClockIn::getPendingEventCount() const noexcept {
    const int read = queueReadIndex.load(std::memory_order_acquire);
    const int write = queueWriteIndex.load(std::memory_order_acquire);

    return (write + queueCapacity - read) % queueCapacity;
}

std::int64_t ClockIn::getDroppedEventCount() const noexcept {
    return droppedEvents.load(std::memory_order_relaxed);
}

ClockIn::Snapshot ClockIn::snapshot() const noexcept {
    Snapshot result;

    result.locked = locked.load(std::memory_order_relaxed);
    result.playing = playing.load(std::memory_order_relaxed);
    result.clockPresent = clockPresent.load(std::memory_order_relaxed);
    result.bpm = bpm.load(std::memory_order_relaxed);
    result.ppq = ppq.load(std::memory_order_relaxed);
    result.tickCount = tickCount.load(std::memory_order_relaxed);
    result.clockCount = clockCount.load(std::memory_order_relaxed);
    result.secondsSinceClock = secondsSinceClock.load(std::memory_order_relaxed);

    return result;
}

double ClockIn::clampBpm(double tempo) noexcept {
    return std::clamp(tempo, Transport::minBpm, Transport::maxBpm);
}

double ClockIn::tickPeriodSeconds(double tempo) noexcept {
    return 60.0 / (tempo * pulsesPerQuarterNote);
}

}  // namespace AudioEngine
