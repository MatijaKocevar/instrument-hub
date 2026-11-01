#pragma once

#include "Transport.h"

#include <atomic>
#include <cstdint>

namespace AudioEngine
{

class MidiSink
{
public:
    virtual ~MidiSink() = default;

    virtual void sendRealTime (std::uint8_t statusByte) noexcept = 0;
};

class ClockOut final
{
public:
    static constexpr int pulsesPerQuarterNote = 24;

    enum class Type : std::uint8_t
    {
        clock     = 0xF8,
        start     = 0xFA,
        continue_ = 0xFB,
        stop      = 0xFC
    };

    struct Event
    {
        Type type = Type::clock;

        int sampleOffset = 0;

        std::int64_t transportSample = 0;

        std::int64_t tick = -1;

        double ppq = 0.0;

        std::uint8_t midiByte() const noexcept
        {
            return static_cast<std::uint8_t> (type);
        }
    };

    static constexpr int maxEventsPerBlock = 256;

    static constexpr int queueCapacity = 2048;

    ClockOut() = default;
    ~ClockOut() = default;

    ClockOut (const ClockOut&) = delete;
    ClockOut& operator= (const ClockOut&) = delete;

    void reset() noexcept;

    int clearQueue() noexcept;

    void process (const Transport& transport, int numSamples) noexcept;

    int generate (const Transport& transport,
                  int numSamples,
                  Event* destination,
                  int capacity) noexcept;

    int drain (Event* destination, int capacity) noexcept;

    int drainTo (MidiSink& sink) noexcept;

    int getPendingEventCount() const noexcept;

    std::int64_t getDroppedEventCount() const noexcept;

    bool isRunning() const noexcept;

    std::int64_t getNextTick() const noexcept;

private:
    static std::int64_t firstTickAfter (double ppq) noexcept;

    bool push (const Event& event) noexcept;

    std::atomic<bool> wasPlaying { false };
    double lastPpq = 0.0;
    std::atomic<std::int64_t> nextTick { 1 };

    Event queue[queueCapacity] {};
    std::atomic<int> queueReadIndex { 0 };
    std::atomic<int> queueWriteIndex { 0 };
    std::atomic<std::int64_t> droppedEvents { 0 };
};

}
