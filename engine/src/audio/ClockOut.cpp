#include "ClockOut.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace AudioEngine {

namespace {
constexpr double ppqTolerance = 1.0e-9;

constexpr double discontinuityTolerancePpq = 1.0e-6;
}  // namespace

void ClockOut::reset() noexcept {
    clearQueue();

    wasPlaying.store(false, std::memory_order_relaxed);
    lastPpq = 0.0;
    nextTick.store(1, std::memory_order_relaxed);
    droppedEvents.store(0, std::memory_order_relaxed);
}

int ClockOut::clearQueue() noexcept {
    const int read = queueReadIndex.load(std::memory_order_relaxed);
    const int write = queueWriteIndex.load(std::memory_order_acquire);
    const int pending = (write + queueCapacity - read) % queueCapacity;

    queueReadIndex.store(write, std::memory_order_release);
    return pending;
}

void ClockOut::process(const Transport& transport, int numSamples) noexcept {
    Event events[maxEventsPerBlock];
    const int count = generate(transport, numSamples, events, maxEventsPerBlock);

    for (int index = 0; index < count; ++index) push(events[index]);
}

int ClockOut::generate(const Transport& transport, int numSamples, Event* destination,
                       int capacity) noexcept {
    if (numSamples <= 0 || destination == nullptr || capacity <= 0) return 0;

    const double sampleRate = transport.getSampleRate();

    if (!(sampleRate > 0.0)) return 0;

    int count = 0;

    const auto emit = [&](Type type, int sampleOffset, std::int64_t transportSample,
                          std::int64_t tick, double ppq) {
        if (count < capacity) {
            auto& event = destination[count++];
            event.type = type;
            event.sampleOffset = sampleOffset;
            event.transportSample = transportSample;
            event.tick = tick;
            event.ppq = ppq;
        } else {
            droppedEvents.fetch_add(1, std::memory_order_relaxed);
        }
    };

    const bool playing = transport.isPlaying();
    const double ppqStart = transport.getPpq();
    const auto positionStart = transport.getPositionSamples();

    if (!playing) {
        if (wasPlaying.load(std::memory_order_relaxed)) {
            emit(Type::stop, 0, positionStart, -1, ppqStart);
            wasPlaying.store(false, std::memory_order_relaxed);
        }

        nextTick.store(firstTickAfter(ppqStart), std::memory_order_relaxed);
        lastPpq = ppqStart;
        return count;
    }

    const bool wasRunning = wasPlaying.load(std::memory_order_relaxed);

    if (!wasRunning) {
        const bool atZero = ppqStart <= ppqTolerance;

        emit(atZero ? Type::start : Type::continue_, 0, positionStart, -1, ppqStart);

        nextTick.store(firstTickAfter(ppqStart), std::memory_order_relaxed);
        wasPlaying.store(true, std::memory_order_relaxed);
        lastPpq = ppqStart;
    } else if (std::abs(ppqStart - lastPpq) > discontinuityTolerancePpq) {
        nextTick.store(firstTickAfter(ppqStart), std::memory_order_relaxed);
    }

    const double ppqEnd = transport.getPpqAtSampleOffset(numSamples);
    const double samplesPerQuarterNote = transport.getSamplesPerQuarterNote();

    if (samplesPerQuarterNote > 0.0) {
        const double endTick = ppqEnd * pulsesPerQuarterNote;

        while (static_cast<double>(nextTick.load(std::memory_order_relaxed)) <=
               endTick + ppqTolerance) {
            const auto tick = nextTick.load(std::memory_order_relaxed);
            const double tickPpq = static_cast<double>(tick) / pulsesPerQuarterNote;

            const double offsetFrames = (tickPpq - ppqStart) * samplesPerQuarterNote;
            const auto roundedOffset = static_cast<std::int64_t>(std::llround(offsetFrames));

            const int sampleOffset =
                static_cast<int>(std::clamp<std::int64_t>(roundedOffset, 0, numSamples - 1));

            emit(Type::clock, sampleOffset, positionStart + roundedOffset, tick, tickPpq);

            nextTick.store(tick + 1, std::memory_order_relaxed);
        }
    }

    lastPpq = ppqEnd;
    return count;
}

int ClockOut::drain(Event* destination, int capacity) noexcept {
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

int ClockOut::drainTo(MidiSink& sink) noexcept {
    int total = 0;
    Event buffer[64];

    for (;;) {
        const int count = drain(buffer, 64);

        for (int index = 0; index < count; ++index) sink.sendRealTime(buffer[index].midiByte());

        total += count;

        if (count < 64) return total;
    }
}

int ClockOut::getPendingEventCount() const noexcept {
    const int read = queueReadIndex.load(std::memory_order_acquire);
    const int write = queueWriteIndex.load(std::memory_order_acquire);

    return (write + queueCapacity - read) % queueCapacity;
}

std::int64_t ClockOut::getDroppedEventCount() const noexcept {
    return droppedEvents.load(std::memory_order_relaxed);
}

bool ClockOut::isRunning() const noexcept { return wasPlaying.load(std::memory_order_relaxed); }

std::int64_t ClockOut::getNextTick() const noexcept {
    return nextTick.load(std::memory_order_relaxed);
}

std::int64_t ClockOut::firstTickAfter(double ppq) noexcept {
    if (!std::isfinite(ppq) || ppq <= 0.0) return 1;

    const double scaled = ppq * pulsesPerQuarterNote;

    if (scaled >= static_cast<double>(std::numeric_limits<std::int64_t>::max() - 1))
        return std::numeric_limits<std::int64_t>::max();

    return static_cast<std::int64_t>(std::floor(scaled + ppqTolerance)) + 1;
}

bool ClockOut::push(const Event& event) noexcept {
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

}  // namespace AudioEngine
