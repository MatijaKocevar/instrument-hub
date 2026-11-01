#include "Transport.h"

#include "Fixtures.h"

namespace AudioTest::ClockOut
{

void runTransport()
{
    std::cout << "\n-- start, stop and continue --" << std::endl;

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 128;

    AudioEngine::Transport transport;
    transport.prepare (sampleRate, blockSize);
    transport.setBpm (120.0);

    AudioEngine::ClockOut clock;

    // Start at zero: exactly one start, no clock in the first 128 frames.
    transport.start();
    auto first = render (clock, transport, 128, blockSize);

    check (first.size() == 1 && first.front().type == AudioEngine::ClockOut::Type::start,
           "starting at zero emits start and no premature clock");

    // The rest of one second: 48 clocks at 120 BPM.
    auto firstSecond = render (clock, transport, 48000 - 128, blockSize);
    check (countClocks (firstSecond) == 48,
           "the first second emits 48 clocks (24 PPQN, 120 BPM)");

    // Stop: one stop message at the current position, and none on later blocks.
    transport.stop();
    auto stopped = render (clock, transport, blockSize, blockSize);
    check (stopped.size() == 1 && stopped.front().type == AudioEngine::ClockOut::Type::stop,
           "stopping emits exactly one stop");
    check (nearlyEqual (stopped.front().ppq, 2.0, 1.0e-9),
           "stop is timestamped at the position where playback stopped");

    auto stillStopped = render (clock, transport, blockSize * 4, blockSize);
    check (stillStopped.empty(), "a stopped transport emits nothing further");

    // Continue: playback resumes at PPQ 2, so the message is continue.
    transport.start();
    auto resumed = render (clock, transport, 128, blockSize);
    check (resumed.size() == 1
               && resumed.front().type == AudioEngine::ClockOut::Type::continue_,
           "restarting at a non-zero position emits continue");

    auto nextSecond = render (clock, transport, 48000, blockSize);
    check (countClocks (nextSecond) == 48,
           "continue keeps the clock grid running (48 clocks per second)");

    bool ticksAfterContinue = true;
    std::int64_t expected = 49;

    for (const auto& event : resumed)
    {
        if (event.type == AudioEngine::ClockOut::Type::clock && event.tick != expected)
            ticksAfterContinue = false;
    }

    for (const auto& event : nextSecond)
    {
        if (event.type != AudioEngine::ClockOut::Type::clock)
            continue;

        if (event.tick != expected)
            ticksAfterContinue = false;

        ++expected;
    }

    check (ticksAfterContinue && expected == 97,
           "continue resumes on tick 49 and ends on tick 96");

    // Rewind to zero and start again: start, not continue.
    transport.stop();
    render (clock, transport, blockSize, blockSize);   // stop message
    transport.rewind();
    transport.start();
    auto afterRewind = render (clock, transport, blockSize, blockSize);
    check (afterRewind.size() == 1
               && afterRewind.front().type == AudioEngine::ClockOut::Type::start,
           "starting from zero after a rewind emits start");
}

} // namespace AudioTest::ClockOut
