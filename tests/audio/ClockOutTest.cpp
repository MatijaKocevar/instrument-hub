// ihub-clock-out-test: acceptance check for T072 (MIDI clock out).
//
// The acceptance sentence is:
//
//     10 s at 120 BPM emits exactly 480 clock messages, with jitter under 1 ms.
//
// The transport is sample accurate, so the test feeds exactly the frames a
// 10 s render would process and measures the timestamp of every clock against
// the ideal 1000-frame grid (120 BPM, 48 kHz). It also covers start, stop and
// continue, rewind/reposition, tempo changes, other sample rates and block
// sizes, the lock-free output queue, the MIDI bytes, and allocation freedom.

#include "audio/ClockOut.h"
#include "audio/Transport.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <new>
#include <string>
#include <vector>

//==============================================================================
// Counting allocator: only the audio-thread loop runs with counting enabled.

namespace allocCount
{
std::atomic<bool> enabled { false };
std::atomic<long> total { 0 };

inline void note() noexcept
{
    if (enabled.load (std::memory_order_relaxed))
        total.fetch_add (1, std::memory_order_relaxed);
}
} // namespace allocCount

void* operator new (std::size_t size)
{
    allocCount::note();

    if (void* pointer = std::malloc (size))
        return pointer;

    throw std::bad_alloc();
}

void* operator new[] (std::size_t size)
{
    return ::operator new (size);
}

void operator delete (void* pointer) noexcept { std::free (pointer); }
void operator delete[] (void* pointer) noexcept { std::free (pointer); }
void operator delete (void* pointer, std::size_t) noexcept { std::free (pointer); }
void operator delete[] (void* pointer, std::size_t) noexcept { std::free (pointer); }

void* operator new (std::size_t size, std::align_val_t alignment)
{
    allocCount::note();

    const auto align = static_cast<std::size_t> (alignment);
    const std::size_t rounded = size == 0 ? align : ((size + align - 1) / align) * align;

    if (void* pointer = std::aligned_alloc (align, rounded))
        return pointer;

    throw std::bad_alloc();
}

void* operator new[] (std::size_t size, std::align_val_t alignment)
{
    return ::operator new (size, alignment);
}

void operator delete (void* pointer, std::align_val_t) noexcept { std::free (pointer); }
void operator delete[] (void* pointer, std::align_val_t) noexcept { std::free (pointer); }
void operator delete (void* pointer, std::size_t, std::align_val_t) noexcept { std::free (pointer); }
void operator delete[] (void* pointer, std::size_t, std::align_val_t) noexcept { std::free (pointer); }

namespace
{
int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

bool nearlyEqual (double a, double b, double tolerance)
{
    return std::abs (a - b) <= tolerance;
}

struct RecordingSink final : AudioEngine::MidiSink
{
    void sendRealTime (std::uint8_t statusByte) noexcept override
    {
        bytes.push_back (statusByte);
    }

    std::vector<std::uint8_t> bytes;
};

/** Runs `frames` of audio through the transport and clock, collecting events.

    `process` is called before `transport.process`, exactly as documented.
*/
std::vector<AudioEngine::ClockOut::Event> render (AudioEngine::ClockOut& clock,
                                                  AudioEngine::Transport& transport,
                                                  std::int64_t frames,
                                                  int blockSize)
{
    std::vector<AudioEngine::ClockOut::Event> events;
    events.reserve (static_cast<std::size_t> (frames / blockSize + 64));

    std::vector<AudioEngine::ClockOut::Event> drained (64);

    while (frames > 0)
    {
        const int block = static_cast<int> (
            std::min<std::int64_t> (frames, static_cast<std::int64_t> (blockSize)));

        clock.process (transport, block);

        for (;;)
        {
            const int count = clock.drain (drained.data(), static_cast<int> (drained.size()));

            for (int index = 0; index < count; ++index)
                events.push_back (drained[static_cast<std::size_t> (index)]);

            if (count < static_cast<int> (drained.size()))
                break;
        }

        transport.process (block);
        frames -= block;
    }

    return events;
}

int countClocks (const std::vector<AudioEngine::ClockOut::Event>& events)
{
    return static_cast<int> (std::count_if (events.begin(), events.end(),
                                            [] (const auto& event)
                                            {
                                                return event.type == AudioEngine::ClockOut::Type::clock;
                                            }));
}

/** Worst deviation of a clock event from its ideal transport frame.

    Returns samples. `samplesPerTick` is derived from the tempo.
*/
double worstJitterSamples (const std::vector<AudioEngine::ClockOut::Event>& events,
                           double samplesPerTick)
{
    double worst = 0.0;

    for (const auto& event : events)
    {
        if (event.type != AudioEngine::ClockOut::Type::clock)
            continue;

        const double ideal = static_cast<double> (event.tick) * samplesPerTick;
        worst = std::max (worst, std::abs (static_cast<double> (event.transportSample) - ideal));
    }

    return worst;
}

//==============================================================================
void testAcceptance()
{
    std::cout << "\n-- acceptance: 10 s at 120 BPM emits 480 clocks --" << std::endl;

    constexpr double sampleRate = 48000.0;
    constexpr double bpm = 120.0;
    constexpr int blockSize = 128;
    constexpr std::int64_t frames = 480000;
    constexpr double samplesPerTick = sampleRate * 60.0 / (bpm * 24.0);   // 1000

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

    // Ticks must be the contiguous range 1..480. Tick zero is the start
    // position and is covered by the start message, not a clock.
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

//==============================================================================
void testStartStopAndContinue()
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

//==============================================================================
void testTempoAndFormatChanges()
{
    std::cout << "\n-- tempo and device formats --" << std::endl;

    {
        AudioEngine::Transport transport;
        transport.prepare (48000.0, 256);
        transport.setBpm (60.0);
        transport.start();

        AudioEngine::ClockOut clock;
        const auto events = render (clock, transport, 48000, 256);

        check (countClocks (events) == 24,
               "one second at 60 BPM emits 24 clocks");

        const double worst = worstJitterSamples (events, 48000.0 * 60.0 / (60.0 * 24.0));
        check (worst < 48.0, "60 BPM clocks stay within 1 ms");
    }

    {
        AudioEngine::Transport transport;
        transport.prepare (44100.0, 512);
        transport.setBpm (120.0);
        transport.start();

        AudioEngine::ClockOut clock;
        const auto events = render (clock, transport, 441000, 512);

        check (countClocks (events) == 480,
               "10 s at 44.1 kHz still emits 480 clocks");

        const double samplesPerTick = 44100.0 * 60.0 / (120.0 * 24.0);
        const double worst = worstJitterSamples (events, samplesPerTick);

        std::cout << "44.1 kHz worst jitter: " << worst << " samples ("
                  << worst * 1000.0 / 44100.0 << " ms)" << std::endl;

        check (worst < 44.1, "44.1 kHz jitter is under 1 ms (44.1 samples)");
    }

    {
        AudioEngine::Transport transport;
        transport.prepare (96000.0, 1024);
        transport.setBpm (180.0);
        transport.start();

        AudioEngine::ClockOut clock;
        const auto events = render (clock, transport, 96000, 1024);

        check (countClocks (events) == 72,
               "one second at 180 BPM emits 72 clocks (3 beats x 24)");

        const double worst = worstJitterSamples (events, 96000.0 * 60.0 / (180.0 * 24.0));
        check (worst < 96.0, "180 BPM at 96 kHz stays within 1 ms");
    }

    // A tempo change between blocks preserves tick continuity.
    {
        AudioEngine::Transport transport;
        transport.prepare (48000.0, 128);
        transport.setBpm (120.0);
        transport.start();

        AudioEngine::ClockOut clock;
        auto first = render (clock, transport, 48000, 128);
        check (countClocks (first) == 48, "120 BPM first second is 48 clocks");

        transport.setBpm (60.0);
        auto second = render (clock, transport, 48000, 128);
        check (countClocks (second) == 24, "a tempo change to 60 BPM halves the clock rate");

        bool continuous = true;
        std::int64_t expected = 49;

        for (const auto& event : second)
        {
            if (event.type != AudioEngine::ClockOut::Type::clock)
                continue;

            if (event.tick != expected)
                continuous = false;

            ++expected;
        }

        check (continuous && expected == 73,
               "the tick counter continues across the tempo change");
    }
}

//==============================================================================
void testRewindWhilePlaying()
{
    std::cout << "\n-- rewind/reposition while playing --" << std::endl;

    AudioEngine::Transport transport;
    transport.prepare (48000.0, 128);
    transport.setBpm (120.0);
    transport.start();

    AudioEngine::ClockOut clock;
    render (clock, transport, 48000, 128);   // ticks 1..48

    transport.setPpq (10.0);                 // jump while running

    auto jumpBlock = render (clock, transport, 128, 128);
    check (jumpBlock.empty(),
           "repositioning does not emit a burst of skipped ticks");

    auto afterJump = render (clock, transport, 48000, 128);

    bool startsAt241 = ! afterJump.empty()
                       && afterJump.front().type == AudioEngine::ClockOut::Type::clock
                       && afterJump.front().tick == 241;

    check (startsAt241, "the generator resynchronises to tick 241 at PPQ 10");
    check (countClocks (afterJump) == 48,
           "one second after the jump emits 48 clocks at 120 BPM");
}

//==============================================================================
void testMidiBytesAndSink()
{
    std::cout << "\n-- output queue and MIDI bytes --" << std::endl;

    AudioEngine::ClockOut clock;

    check (static_cast<std::uint8_t> (AudioEngine::ClockOut::Type::clock) == 0xF8
               && static_cast<std::uint8_t> (AudioEngine::ClockOut::Type::start) == 0xFA
               && static_cast<std::uint8_t> (AudioEngine::ClockOut::Type::continue_) == 0xFB
               && static_cast<std::uint8_t> (AudioEngine::ClockOut::Type::stop) == 0xFC,
           "message values are the MIDI system real-time bytes");

    AudioEngine::Transport transport;
    transport.prepare (48000.0, 128);
    transport.setBpm (120.0);
    transport.start();

    // 1 s playing, then stop; drain everything through a sink.
    for (int block = 0; block < 375; ++block)
    {
        clock.process (transport, 128);
        transport.process (128);
    }

    transport.stop();
    clock.process (transport, 128);

    RecordingSink sink;
    const int sent = clock.drainTo (sink);

    check (sent == 50, "drainTo forwards start, 48 clocks and stop");
    check (! sink.bytes.empty() && sink.bytes.front() == 0xFA,
           "the first byte is start (0xFA)");
    check (! sink.bytes.empty() && sink.bytes.back() == 0xFC,
           "the last byte is stop (0xFC)");
    check (static_cast<int> (std::count (sink.bytes.begin(), sink.bytes.end(),
                                         static_cast<std::uint8_t> (0xF8))) == 48,
           "48 clock bytes (0xF8) were sent");
    check (clock.getPendingEventCount() == 0, "drainTo empties the queue");
    check (clock.drainTo (sink) == 0, "a second drain sends nothing");
}

//==============================================================================
void testProcessAllocationFree()
{
    std::cout << "\n-- process() is allocation free --" << std::endl;

    AudioEngine::Transport transport;
    transport.prepare (48000.0, 128);
    transport.setBpm (120.0);
    transport.start();

    AudioEngine::ClockOut clock;
    clock.process (transport, 128);   // start message
    clock.clearQueue();

    allocCount::total.store (0, std::memory_order_relaxed);
    allocCount::enabled.store (true, std::memory_order_relaxed);

    for (int block = 0; block < 10000; ++block)
    {
        clock.process (transport, 128);
        transport.process (128);

        // Keep the queue from filling: draining is a message-thread operation.
        allocCount::enabled.store (false, std::memory_order_relaxed);
        clock.clearQueue();
        allocCount::enabled.store (true, std::memory_order_relaxed);
    }

    allocCount::enabled.store (false, std::memory_order_relaxed);

    std::cout << "allocations during 10000 process() calls: "
              << allocCount::total.load (std::memory_order_relaxed) << std::endl;

    check (allocCount::total.load (std::memory_order_relaxed) == 0,
           "10000 process() calls allocate nothing");
    check (clock.getDroppedEventCount() == 0,
           "clearQueue between blocks loses no events");
}

//==============================================================================
void testSmallBufferReportsOverflow()
{
    std::cout << "\n-- small output buffer --" << std::endl;

    AudioEngine::Transport transport;
    transport.prepare (48000.0, 9600);
    transport.setBpm (120.0);
    transport.start();

    AudioEngine::ClockOut clock;

    // First block: start plus ticks 1..9. Then a block that needs ten clocks
    // but is given room for one, to prove the overflow is counted and the tick
    // grid still advances.
    AudioEngine::ClockOut::Event roomy[256];
    const int firstCount = clock.generate (transport, 9600, roomy, 256);
    check (firstCount == 10, "a 9600-frame block at 120 BPM carries start and 9 clocks");

    transport.process (9600);

    const auto droppedBefore = clock.getDroppedEventCount();

    AudioEngine::ClockOut::Event tiny[1];
    const int written = clock.generate (transport, 9600, tiny, 1);

    check (written == 1, "generate never writes past the caller's capacity");
    check (clock.getDroppedEventCount() - droppedBefore == 9,
           "events that do not fit are counted as dropped");
    check (clock.getNextTick() == 20,
           "the tick counter advances past the skipped events");

    // A zero-capacity call is a no-op, not a crash.
    check (clock.generate (transport, 128, nullptr, 0) == 0,
           "a null/zero-capacity destination is ignored");
}

} // namespace

int main()
{
    testAcceptance();
    testStartStopAndContinue();
    testTempoAndFormatChanges();
    testRewindWhilePlaying();
    testMidiBytesAndSink();
    testProcessAllocationFree();
    testSmallBufferReportsOverflow();

    if (failures == 0)
    {
        std::cout << "\nPASS: clock out" << std::endl;
        return 0;
    }

    std::cout << "\nFAIL: " << failures << " check(s) failed" << std::endl;
    return 1;
}
