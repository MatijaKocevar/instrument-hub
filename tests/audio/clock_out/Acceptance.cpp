#include "Acceptance.h"

#include "Fixtures.h"

namespace AudioTest::ClockOut
{

void runAcceptance()
{
    std::cout << "\n-- acceptance: 10 s at 120 BPM emits 480 clocks --" << std::endl;

    constexpr double sampleRate = 48000.0;
    constexpr double bpm = 120.0;
    constexpr int blockSize = 128;
    constexpr std::int64_t frames = 480000;
    constexpr double samplesPerTick = sampleRate * 60.0 / (bpm * 24.0);

    AudioEngine::Transport transport;
    transport.prepare (sampleRate, blockSize);
    transport.setBpm (bpm);
    transport.start();

    AudioEngine::ClockOut clock;
    const auto events = render (clock, transport, frames, blockSize);

    const int clocks = countClocks (events);

    std::cout << "clock messages after 10 s: " << clocks << " (expected 480)" << std::endl;

    check (clocks == 480, "10 s at 120 BPM emits exactly 480 clock messages");
    check (clock.getPendingEventCount() == 0, "the message-thread queue is fully drained");
    check (clock.getDroppedEventCount() == 0, "no event was dropped");

    check (! events.empty() && events.front().type == AudioEngine::ClockOut::Type::start,
           "the first message is start");
    check (events.front().ppq == 0.0 && events.front().sampleOffset == 0,
           "start is timestamped at position zero");

    const double worst = worstJitterSamples (events, samplesPerTick);
    const double worstMs = worst * 1000.0 / sampleRate;

    std::cout << "worst clock jitter: " << worst << " samples ("
              << worstMs << " ms)" << std::endl;

    check (worst < sampleRate * 0.001,
           "clock jitter is under 1 ms (48 samples at 48 kHz)");

    bool ticksContiguous = true;
    bool offsetsValid = true;
    std::int64_t expectedTick = 1;

    for (const auto& event : events)
    {
        if (event.type != AudioEngine::ClockOut::Type::clock)
            continue;

        if (event.tick != expectedTick)
            ticksContiguous = false;

        ++expectedTick;

        if (event.sampleOffset < 0 || event.sampleOffset >= blockSize)
            offsetsValid = false;

        if (! nearlyEqual (event.ppq, static_cast<double> (event.tick) / 24.0, 1.0e-12))
            offsetsValid = false;
    }

    check (ticksContiguous && expectedTick == 481,
           "clock tick indices are the contiguous range 1..480");
    check (offsetsValid,
           "every event has a valid in-block offset and a 24-PPQN position");
}

}
