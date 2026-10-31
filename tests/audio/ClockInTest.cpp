// ihub-clock-in-test: acceptance check for T073 (MIDI clock in).
//
// The acceptance sentence is:
//
//     feeding 120 BPM clock from the simulator locks the transport within 2 s.
//
// The simulator here is a frame-accurate MIDI clock master: it emits the
// system real-time bytes at the frame positions a 120 BPM master would use,
// with timestamps derived from the same frame timeline, and pushes them into
// ClockIn exactly like a MIDI callback would. The transport then has to adopt
// that clock. Keeping the source frame-based matches T072's offline render and
// keeps the test independent of wall-clock timing, as plan/TESTING.md requires.
//
// Also covered: 60/90/180/240 BPM, timestamp jitter, start/stop/continue,
// position hold while stopped, clock loss without a stop message, relock,
// non-real-time bytes, queue overflow accounting, MIDI byte values, and
// allocation-free processing.

#include "audio/ClockIn.h"
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

constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 128;

/** One beat expressed in quarter notes. */
constexpr double tick = 1.0 / 24.0;

//==============================================================================
/** A deterministic, frame-timed MIDI clock master.

    `advance()` pushes every clock byte that falls inside the next block, using
    the frame position as the arrival timestamp, then runs the follower and
    the transport in the documented order. The tick schedule is stored as a
    double so a tempo change never accumulates rounding error.
*/
class Simulator
{
public:
    Simulator (double sampleRate, double tempo)
        : rate (sampleRate), bpmValue (tempo)
    {
    }

    void start()
    {
        clockIn.pushRealtime (static_cast<std::uint8_t> (AudioEngine::ClockIn::Type::start),
                              timestampOf (frame));
        running = true;
        nextTickFrame = static_cast<double> (frame) + tickFrames();
    }

    /** Sends stop. A real master keeps sending clock after stop, so this
        leaves the tick stream running unless `disappear()` is used.
    */
    void stop()
    {
        clockIn.pushRealtime (static_cast<std::uint8_t> (AudioEngine::ClockIn::Type::stop),
                              timestampOf (frame));
    }

    void resume()
    {
        clockIn.pushRealtime (static_cast<std::uint8_t> (AudioEngine::ClockIn::Type::continue_),
                              timestampOf (frame));
        running = true;
        nextTickFrame = static_cast<double> (frame) + tickFrames();
    }

    /** Changes the tempo from the next tick on. */
    void setTempo (double tempo)
    {
        bpmValue = tempo;
        nextTickFrame = static_cast<double> (frame) + tickFrames();
    }

    /** Stops sending clock bytes without sending a stop message. */
    void disappear() { running = false; }

    void setJitter (double amplitudeSeconds, std::uint32_t seed)
    {
        jitter = amplitudeSeconds;
        randomState = seed;
    }

    void advance (AudioEngine::Transport& transport, int block)
    {
        const auto end = frame + block;

        if (running)
        {
            while (nextTickFrame < static_cast<double> (end))
            {
                const double timestamp = nextTickFrame / rate + nextJitter();

                clockIn.pushRealtime (
                    static_cast<std::uint8_t> (AudioEngine::ClockIn::Type::clock),
                    timestamp);

                nextTickFrame += tickFrames();
            }
        }

        clockIn.process (transport, block);
        transport.process (block);
        frame = end;
    }

    /** Advances whole blocks until `frames` have been processed. */
    void run (AudioEngine::Transport& transport, std::int64_t frames, int block)
    {
        while (frames > 0)
        {
            const int step = static_cast<int> (
                std::min<std::int64_t> (frames, static_cast<std::int64_t> (block)));

            advance (transport, step);
            frames -= step;
        }
    }

    std::int64_t frameIndex() const noexcept { return frame; }

    AudioEngine::ClockIn clockIn;

private:
    double tickFrames() const noexcept { return rate * 60.0 / (bpmValue * 24.0); }

    double timestampOf (std::int64_t atFrame) const noexcept
    {
        return static_cast<double> (atFrame) / rate;
    }

    double nextJitter() noexcept
    {
        if (jitter <= 0.0)
            return 0.0;

        randomState = randomState * 1664525u + 1013904223u;
        const double unit = static_cast<double> ((randomState >> 8) & 0xFFFFu) / 65535.0;

        return (unit * 2.0 - 1.0) * jitter;
    }

    double rate;
    double bpmValue;
    double jitter = 0.0;
    std::uint32_t randomState = 1;
    std::int64_t frame = 0;
    double nextTickFrame = 0.0;
    bool running = false;
};

/** A transport in external-sync mode at the test format. */
struct Fixture
{
    AudioEngine::Transport transport;

    Fixture()
    {
        transport.prepare (kSampleRate, kBlockSize);
        transport.setSyncSource (AudioEngine::Transport::SyncSource::external);
    }
};

//==============================================================================
void testAcceptance()
{
    std::cout << "\n-- acceptance: 120 BPM clock locks within 2 s --" << std::endl;

    Fixture fixture;
    Simulator simulator (kSampleRate, 120.0);
    simulator.start();

    constexpr std::int64_t twoSeconds = 96000;
    std::int64_t lockedAtFrame = -1;

    for (std::int64_t processed = 0; processed < twoSeconds; processed += kBlockSize)
    {
        simulator.advance (fixture.transport, kBlockSize);

        if (lockedAtFrame < 0 && simulator.clockIn.isLocked())
            lockedAtFrame = simulator.frameIndex();
    }

    const auto& clockIn = simulator.clockIn;

    std::cout << "locked after " << (lockedAtFrame < 0 ? -1.0 : lockedAtFrame / kSampleRate)
              << " s, estimate " << clockIn.getBpm() << " BPM, ppq " << clockIn.getPpq()
              << ", transport ppq " << fixture.transport.getPpq() << std::endl;

    check (lockedAtFrame > 0 && lockedAtFrame <= twoSeconds,
           "the clock locks within 2 s");
    check (nearlyEqual (clockIn.getBpm(), 120.0, 0.01),
           "the tempo estimate is 120 BPM");
    check (clockIn.isPlaying() && clockIn.isClockPresent(),
           "the follower reports a present clock that is playing");

    check (fixture.transport.isPlaying(),
           "the transport is rolling in external sync");
    check (fixture.transport.getSyncSource() == AudioEngine::Transport::SyncSource::external
               && fixture.transport.isExternalClockLocked(),
           "the transport adopted the external clock");
    check (nearlyEqual (fixture.transport.getBpm(), 120.0, 0.01),
           "the transport tempo is 120 BPM");

    // 2 s at 120 BPM is exactly 4 quarter notes. The last tick before the
    // 2 s boundary is tick 95, so one tick of lead is expected.
    check (nearlyEqual (fixture.transport.getPpq(), 4.0, tick),
           "the transport position is 4 quarter notes +/- one tick");
    check (std::abs (clockIn.getTickCount() - 95) <= 1,
           "95 ticks arrived in the first 2 s (96 x 24 PPQN per 2 s)");

    check (clockIn.getDroppedEventCount() == 0,
           "no clock event was dropped");
}

//==============================================================================
void testTempos()
{
    std::cout << "\n-- other tempos --" << std::endl;

    for (const double bpm : { 60.0, 90.0, 180.0, 240.0 })
    {
        Fixture fixture;
        Simulator simulator (kSampleRate, bpm);
        simulator.start();

        // 2.5 s is enough for 24 ticks even at 60 BPM (1 s).
        simulator.run (fixture.transport, 120000, kBlockSize);

        const double expectedTicks = 2.5 * bpm / 60.0 * 24.0;
        const double expectedPpq = 2.5 * bpm / 60.0;

        std::cout << bpm << " BPM -> estimate " << simulator.clockIn.getBpm()
                  << ", ticks " << simulator.clockIn.getTickCount()
                  << ", ppq " << fixture.transport.getPpq() << std::endl;

        check (simulator.clockIn.isLocked(),
               std::to_string (static_cast<int> (bpm)) + " BPM locks");
        check (nearlyEqual (simulator.clockIn.getBpm(), bpm, 0.05),
               std::to_string (static_cast<int> (bpm)) + " BPM is estimated correctly");
        check (nearlyEqual (static_cast<double> (simulator.clockIn.getTickCount()),
                            expectedTicks, 1.5),
               std::to_string (static_cast<int> (bpm)) + " BPM advances one tick per tick");
        check (nearlyEqual (fixture.transport.getPpq(), expectedPpq, tick),
               std::to_string (static_cast<int> (bpm)) + " BPM position matches elapsed time");
    }
}

//==============================================================================
void testTempoChange()
{
    std::cout << "\n-- tempo change while running --" << std::endl;

    Fixture fixture;
    Simulator simulator (kSampleRate, 120.0);
    simulator.start();
    simulator.run (fixture.transport, 96000, kBlockSize);   // 2 s at 120

    check (nearlyEqual (simulator.clockIn.getBpm(), 120.0, 0.05),
           "the first tempo is locked at 120 BPM");

    simulator.setTempo (90.0);
    simulator.run (fixture.transport, 96000, kBlockSize);   // 2 s at 90

    std::cout << "after 120 -> 90 BPM: estimate " << simulator.clockIn.getBpm()
              << ", transport " << fixture.transport.getBpm() << std::endl;

    check (simulator.clockIn.isLocked(),
           "the lock survives a tempo change");
    check (nearlyEqual (simulator.clockIn.getBpm(), 90.0, 0.5),
           "the estimate follows a 120 -> 90 BPM change within 2 s");
    check (nearlyEqual (fixture.transport.getBpm(), 90.0, 0.5),
           "the transport follows the new tempo");
}

//==============================================================================
void testJitter()
{
    std::cout << "\n-- timestamp jitter --" << std::endl;

    Fixture fixture;
    Simulator simulator (kSampleRate, 120.0);
    simulator.setJitter (0.003, 0x5EED1234u);   // +/- 3 ms on a 20.8 ms tick
    simulator.start();

    simulator.run (fixture.transport, 144000, kBlockSize);   // 3 s

    std::cout << "jittered estimate: " << simulator.clockIn.getBpm()
              << " BPM, ticks " << simulator.clockIn.getTickCount() << std::endl;

    check (simulator.clockIn.isLocked(),
           "a jittered clock still locks");
    check (nearlyEqual (simulator.clockIn.getBpm(), 120.0, 1.0),
           "jitter of +/- 3 ms moves the estimate by less than 1 BPM");
    check (std::abs (simulator.clockIn.getTickCount() - 143) <= 1,
           "jitter does not lose or invent ticks");
    check (nearlyEqual (fixture.transport.getPpq(), 6.0, 2.0 * tick),
           "the jittered position stays within two ticks of 3 s of music");
}

//==============================================================================
void testStartStopContinue()
{
    std::cout << "\n-- start, stop and continue --" << std::endl;

    Fixture fixture;
    Simulator simulator (kSampleRate, 120.0);
    simulator.start();

    simulator.run (fixture.transport, 48000, kBlockSize);   // 1 s

    const double heldPpq = fixture.transport.getPpq();
    // Holding is exact to within the block-quantised interpolation phase.
    const double holdTolerance = static_cast<double> (kBlockSize) / (kSampleRate * 60.0 / 120.0);

    check (simulator.clockIn.isPlaying() && fixture.transport.isPlaying(),
           "the transport is playing after one second");
    check (heldPpq > 1.9, "one second at 120 BPM is roughly 2 quarter notes");

    simulator.stop();
    simulator.run (fixture.transport, 2 * kBlockSize, kBlockSize);

    check (! simulator.clockIn.isPlaying(),
           "a stop message clears the playing state");
    check (! fixture.transport.isPlaying(),
           "a stop message stops the transport");
    check (simulator.clockIn.isLocked(),
           "a stopped master that still sends clock keeps the tempo lock");
    check (nearlyEqual (fixture.transport.getPpq(), heldPpq, holdTolerance),
           "the position is held while stopped");

    // The master keeps sending clock while stopped: the position must not move.
    simulator.run (fixture.transport, 48000, kBlockSize);
    check (nearlyEqual (fixture.transport.getPpq(), heldPpq, holdTolerance),
           "clock while stopped does not advance the position");
    check (nearlyEqual (simulator.clockIn.getBpm(), 120.0, 0.05),
           "tempo keeps tracking while stopped");

    simulator.resume();
    simulator.run (fixture.transport, 48000, kBlockSize);

    std::cout << "after continue: transport ppq " << fixture.transport.getPpq() << std::endl;

    check (fixture.transport.isPlaying(),
           "continue restarts the transport");
    check (nearlyEqual (fixture.transport.getPpq(), heldPpq + 2.0, 2.0 * tick),
           "continue resumes from the held position and advances 2 quarter notes");

    // Start rewinds to zero.
    simulator.stop();
    simulator.run (fixture.transport, kBlockSize, kBlockSize);
    simulator.start();
    simulator.run (fixture.transport, kBlockSize, kBlockSize);

    check (simulator.clockIn.getTickCount() <= 1,
           "start resets the tick count");
    check (fixture.transport.getPpq() < 2.0 * tick,
           "start resets the transport to the beginning");
}

//==============================================================================
void testClockLoss()
{
    std::cout << "\n-- clock loss without a stop message --" << std::endl;

    Fixture fixture;
    Simulator simulator (kSampleRate, 120.0);
    simulator.start();
    simulator.run (fixture.transport, 48000, kBlockSize);

    const double heldPpq = fixture.transport.getPpq();
    const double lastClockTime = static_cast<double> (simulator.frameIndex()) / kSampleRate;

    check (fixture.transport.isPlaying(), "the transport is playing before the loss");

    simulator.disappear();
    simulator.run (fixture.transport, 36000, kBlockSize);   // 0.75 s of silence

    std::cout << "after " << (simulator.frameIndex() / kSampleRate - lastClockTime)
              << " s without clock: locked " << simulator.clockIn.isLocked()
              << ", playing " << simulator.clockIn.isPlaying() << std::endl;

    check (! simulator.clockIn.isClockPresent(),
           "silence is detected as a lost clock");
    check (! simulator.clockIn.isLocked(),
           "a lost clock drops the tempo lock");
    check (! fixture.transport.isPlaying(),
           "a lost clock stops the transport");
    check (! fixture.transport.isExternalClockLocked(),
           "the transport reports the external clock as lost");

    // A slave rolls on until it has missed the timeout; it must not run away.
    const double stoppedPpq = fixture.transport.getPpq();
    check (stoppedPpq >= heldPpq && stoppedPpq <= heldPpq + 1.1,
           "the transport rolled only through the detection window");

    simulator.run (fixture.transport, 48000, kBlockSize);

    check (fixture.transport.getPpq() == stoppedPpq,
           "after the loss is detected the position no longer advances");

    // Coming back requires a continue (or start) and a fresh lock.
    simulator.resume();
    simulator.run (fixture.transport, 36000, kBlockSize);

    check (simulator.clockIn.isLocked(),
           "the clock relocks after it returns");
    check (fixture.transport.isPlaying(),
           "the transport resumes after the clock returns");

    // The stop timeout must not fire while the clock keeps arriving, even at
    // the slowest supported tempo. 20 BPM is one tick every 125 ms.
    Fixture slow;
    Simulator slowSimulator (kSampleRate, 20.0);
    slowSimulator.start();
    slowSimulator.run (slow.transport, 192000, kBlockSize);   // 4 s

    check (slowSimulator.clockIn.isClockPresent(),
           "a 20 BPM clock never trips the loss timeout");
}

//==============================================================================
void testIgnoredMessages()
{
    std::cout << "\n-- non-real-time bytes and queue overflow --" << std::endl;

    AudioEngine::ClockIn clockIn;

    check (! clockIn.pushRealtime (0x90, 0.0), "a note-on byte is ignored");
    check (! clockIn.pushRealtime (0xB0, 0.0), "a control-change byte is ignored");
    check (! clockIn.pushRealtime (0xF0, 0.0), "a SysEx start byte is ignored");
    check (clockIn.pushRealtime (0xF8, 0.0), "a clock byte is accepted");
    check (clockIn.getPendingEventCount() == 1, "the accepted byte is queued");
    check (clockIn.clearQueue() == 1, "clearQueue reports the pending event");
    check (clockIn.getPendingEventCount() == 0, "the queue is empty again");

    // Fill the queue: one slot is always kept free for the SPSC invariant.
    const int capacity = AudioEngine::ClockIn::queueCapacity;
    int accepted = 0;

    for (int index = 0; index < capacity + 16; ++index)
    {
        if (clockIn.pushRealtime (0xF8, static_cast<double> (index) * 0.02))
            ++accepted;
    }

    check (accepted == capacity - 1,
           "the queue accepts exactly capacity-1 events");
    check (clockIn.getDroppedEventCount() == 17,
           "the overflow is counted as dropped events");
    check (clockIn.getPendingEventCount() == capacity - 1,
           "every accepted event is pending");

    Fixture fixture;
    for (int block = 0; block < 64; ++block)
        clockIn.process (fixture.transport, kBlockSize);

    check (clockIn.getPendingEventCount() == 0,
           "process() drains the queue completely");
}

//==============================================================================
void testInternalSyncIsUntouched()
{
    std::cout << "\n-- internal sync is untouched --" << std::endl;

    AudioEngine::Transport transport;
    transport.prepare (kSampleRate, kBlockSize);
    transport.setBpm (100.0);

    Simulator simulator (kSampleRate, 120.0);
    simulator.start();
    simulator.run (transport, 96000, kBlockSize);

    check (simulator.clockIn.isLocked(),
           "the follower still estimates the clock");
    check (! transport.isPlaying(),
           "clock updates do not start a transport in internal sync");
    check (transport.getBpm() == 100.0,
           "clock updates do not change an internal tempo");
    check (transport.getPpq() == 0.0,
           "clock updates do not move an internal position");
}

//==============================================================================
void testMidiBytes()
{
    std::cout << "\n-- MIDI byte values --" << std::endl;

    check (static_cast<std::uint8_t> (AudioEngine::ClockIn::Type::clock) == 0xF8
               && static_cast<std::uint8_t> (AudioEngine::ClockIn::Type::start) == 0xFA
               && static_cast<std::uint8_t> (AudioEngine::ClockIn::Type::continue_) == 0xFB
               && static_cast<std::uint8_t> (AudioEngine::ClockIn::Type::stop) == 0xFC,
           "the message values are the MIDI system real-time bytes");
    check (AudioEngine::ClockIn::pulsesPerQuarterNote == 24,
           "the follower counts 24 pulses per quarter note");
}

//==============================================================================
void testAllocationFree()
{
    std::cout << "\n-- process() is allocation free --" << std::endl;

    Fixture fixture;
    Simulator simulator (kSampleRate, 120.0);
    simulator.start();
    simulator.run (fixture.transport, 48000, kBlockSize);

    check (simulator.clockIn.isLocked(), "the clock is locked before the loop");

    allocCount::total.store (0, std::memory_order_relaxed);
    allocCount::enabled.store (true, std::memory_order_relaxed);

    for (int block = 0; block < 10000; ++block)
    {
        // One tick per four 128-frame blocks at 120 BPM: push the same way the
        // MIDI thread would, then run the audio-thread entry point.
        if (block % 4 == 0)
            simulator.clockIn.pushRealtime (0xF8, static_cast<double> (block) * 128.0 / kSampleRate);

        simulator.clockIn.process (fixture.transport, kBlockSize);
        fixture.transport.process (kBlockSize);
    }

    allocCount::enabled.store (false, std::memory_order_relaxed);

    std::cout << "allocations during 10000 push+process pairs: "
              << allocCount::total.load (std::memory_order_relaxed) << std::endl;

    check (allocCount::total.load (std::memory_order_relaxed) == 0,
           "10000 push/process pairs allocate nothing");
}

//==============================================================================
void testResetAndSnapshot()
{
    std::cout << "\n-- reset and snapshot --" << std::endl;

    Fixture fixture;
    Simulator simulator (kSampleRate, 120.0);
    simulator.start();
    simulator.run (fixture.transport, 96000, kBlockSize);

    auto snapshot = simulator.clockIn.snapshot();

    check (snapshot.locked && snapshot.playing && snapshot.clockPresent,
           "the snapshot reports a locked, playing, present clock");
    check (nearlyEqual (snapshot.bpm, 120.0, 0.01)
               && nearlyEqual (snapshot.ppq, 4.0, tick),
           "the snapshot carries tempo and position");

    simulator.clockIn.reset();

    check (! simulator.clockIn.isLocked()
               && ! simulator.clockIn.isPlaying()
               && ! simulator.clockIn.isClockPresent()
               && simulator.clockIn.getBpm() == 0.0
               && simulator.clockIn.getPpq() == 0.0
               && simulator.clockIn.getTickCount() == 0,
           "reset clears every published field");
    check (simulator.clockIn.getPendingEventCount() == 0,
           "reset clears the queue");
}

} // namespace

//==============================================================================
int main()
{
    testAcceptance();
    testTempos();
    testTempoChange();
    testJitter();
    testStartStopContinue();
    testClockLoss();
    testIgnoredMessages();
    testInternalSyncIsUntouched();
    testMidiBytes();
    testAllocationFree();
    testResetAndSnapshot();

    if (failures == 0)
    {
        std::cout << "\nPASS: clock in" << std::endl;
        return 0;
    }

    std::cout << "\nFAIL: " << failures << " check(s) failed" << std::endl;
    return 1;
}
