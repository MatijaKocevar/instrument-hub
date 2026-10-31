// ihub-transport-test: acceptance check for T071 (sample-accurate transport).
//
// The acceptance sentence is:
//
//     after 10 s at 120 BPM, PPQ is 20.0 +/- 1 tick at 48 kHz.
//
// The transport has no wall clock on the audio path, so the check feeds exactly
// the frames a 10 s render would process. It also covers start/stop/continue,
// rewind/reposition, tempo changes, other sample rates, tap tempo and the
// external-sync hook, and proves `process()` allocates nothing.

#include "audio/Transport.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <string>

//==============================================================================
// Counting allocator: only `process()` runs with counting enabled.

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

constexpr double acceptanceTolerancePpq =
    1.0 / AudioEngine::Transport::ppqTicksPerQuarterNote;

void advance (AudioEngine::Transport& transport, std::int64_t frames, int blockSize)
{
    while (frames > 0)
    {
        const auto block = static_cast<int> (
            std::min<std::int64_t> (frames, static_cast<std::int64_t> (blockSize)));

        transport.process (block);
        frames -= block;
    }
}

void testAcceptance()
{
    std::cout << "\n-- acceptance: 10 s at 120 BPM is 20.0 PPQ --" << std::endl;

    AudioEngine::Transport transport;
    transport.prepare (48000.0, 512);
    transport.setBpm (120.0);
    transport.start();

    advance (transport, 480000, 128);

    const double ppq = transport.getPpq();

    std::cout << "PPQ after 480000 frames: " << ppq << " (expected 20.0 +/- "
              << acceptanceTolerancePpq << ")" << std::endl;

    check (nearlyEqual (ppq, 20.0, acceptanceTolerancePpq),
           "10 s at 120 BPM / 48 kHz gives 20.0 PPQ within one tick");
    check (transport.getPositionSamples() == 480000,
           "position is exactly the 480000 frames that were processed");
    check (nearlyEqual (transport.getSamplesPerQuarterNote(), 24000.0, 1.0e-9),
           "one quarter note is 24000 frames at 120 BPM / 48 kHz");

    // A render with a different block size must give the same answer: the
    // position depends on frames, not on how they were grouped.
    AudioEngine::Transport blocked;
    blocked.prepare (48000.0, 512);
    blocked.setBpm (120.0);
    blocked.start();

    advance (blocked, 480000, 512);

    check (nearlyEqual (blocked.getPpq(), ppq, 1.0e-9),
           "512-frame blocks land on the same PPQ as 128-frame blocks");
}

void testStartStopAndContinue()
{
    std::cout << "\n-- start, stop, continue --" << std::endl;

    AudioEngine::Transport transport;
    transport.prepare (48000.0, 128);
    transport.setBpm (120.0);

    // A stopped transport does not advance, even when frames are processed.
    advance (transport, 48000, 128);
    check (transport.getPpq() == 0.0 && transport.getPositionSamples() == 0,
           "a stopped transport processes frames without moving");

    transport.start();
    advance (transport, 48000, 128);
    check (nearlyEqual (transport.getPpq(), 2.0, 1.0e-12),
           "1 s at 120 BPM advances 2 quarter notes");

    transport.stop();
    advance (transport, 96000, 128);
    check (nearlyEqual (transport.getPpq(), 2.0, 1.0e-12)
               && transport.getPositionSamples() == 48000,
           "stopping holds PPQ and sample position while frames continue");

    transport.start();
    advance (transport, 432000, 128);
    check (nearlyEqual (transport.getPpq(), 20.0, 1.0e-9),
           "continue reaches 20 PPQ after 10 s of playing time");
    check (transport.getPositionSamples() == 480000,
           "total playing frames are 480000");

    transport.setPlaying (true);
    check (transport.isPlaying(), "setPlaying(true) starts the transport");
    transport.setPlaying (false);
    check (! transport.isPlaying(), "setPlaying(false) stops the transport");
}

void testRewindAndReposition()
{
    std::cout << "\n-- rewind and reposition --" << std::endl;

    AudioEngine::Transport transport;
    transport.prepare (48000.0, 128);
    transport.setBpm (120.0);
    transport.start();

    advance (transport, 480000, 128);

    transport.rewind();
    check (transport.getPpq() == 0.0 && transport.getPositionSamples() == 0,
           "rewind clears both positions");
    check (transport.isPlaying() && transport.getBpm() == 120.0,
           "rewind keeps playing state and tempo");

    transport.setPpq (8.0);
    check (transport.getPositionSamples() == 192000,
           "PPQ 8 at 120 BPM / 48 kHz maps to 192000 frames");

    transport.setPositionSamples (24000);
    check (nearlyEqual (transport.getPpq(), 1.0, 1.0e-12),
           "24000 frames map back to PPQ 1");

    transport.setPpq (-3.0);
    check (transport.getPpq() == 0.0 && transport.getPositionSamples() == 0,
           "a negative PPQ is clamped to zero");

    transport.setPositionSamples (-5);
    check (transport.getPositionSamples() == 0,
           "a negative sample position is clamped to zero");
}

void testTempoChangesAndRates()
{
    std::cout << "\n-- tempo changes and sample rates --" << std::endl;

    AudioEngine::Transport transport;
    transport.prepare (48000.0, 128);
    transport.setBpm (60.0);
    transport.start();

    advance (transport, 240000, 128);       // 5 s at 60 BPM -> 5 quarter notes
    check (nearlyEqual (transport.getPpq(), 5.0, 1.0e-12),
           "5 s at 60 BPM advances 5 quarter notes");

    transport.setBpm (120.0);
    advance (transport, 240000, 128);       // 5 s at 120 BPM -> 10 quarter notes
    check (nearlyEqual (transport.getPpq(), 15.0, 1.0e-12),
           "a tempo change applies at the next block and keeps the position");
    check (nearlyEqual (transport.getSamplesPerQuarterNote(), 24000.0, 1.0e-9),
           "the tempo change updates samples per quarter note");

    transport.setBpm (1000.0);
    check (transport.getBpm() == AudioEngine::Transport::maxBpm,
           "tempo is clamped to the 300 BPM maximum");
    transport.setBpm (1.0);
    check (transport.getBpm() == AudioEngine::Transport::minBpm,
           "tempo is clamped to the 20 BPM minimum");
    transport.setBpm (std::numeric_limits<double>::quiet_NaN());
    check (transport.getBpm() == AudioEngine::Transport::minBpm,
           "a non-finite tempo is ignored");

    AudioEngine::Transport fast;
    fast.prepare (96000.0, 256);
    fast.setBpm (120.0);
    fast.start();
    advance (fast, 480000, 256);

    check (nearlyEqual (fast.getPpq(), 10.0, 1.0e-12),
           "480000 frames at 96 kHz are 5 s, i.e. 10 quarter notes at 120 BPM");
    check (nearlyEqual (fast.getSamplesPerQuarterNote(), 48000.0, 1.0e-9),
           "one quarter note is 48000 frames at 120 BPM / 96 kHz");
}

void testSampleOffsetProjection()
{
    std::cout << "\n-- within-block projection --" << std::endl;

    AudioEngine::Transport transport;
    transport.prepare (48000.0, 128);
    transport.setBpm (120.0);

    check (transport.getPpqAtSampleOffset (128) == 0.0,
           "a stopped transport does not project forward");

    transport.start();
    transport.setPpq (4.0);

    const double projected = transport.getPpqAtSampleOffset (128);
    check (nearlyEqual (projected, 4.0 + 128.0 / 24000.0, 1.0e-12),
           "128 frames at 120 BPM project 128/24000 of a quarter note");
    check (transport.getPpq() == 4.0,
           "projecting does not advance the transport");
}

void testTapTempo()
{
    std::cout << "\n-- tap tempo --" << std::endl;

    AudioEngine::Transport transport;
    transport.prepare (48000.0, 128);

    check (transport.tapAt (0.0) == 0.0,
           "one tap is not enough for a tempo estimate");
    check (nearlyEqual (transport.tapAt (0.5), 120.0, 1.0e-12),
           "two taps 500 ms apart give 120 BPM");

    transport.resetTapTempo();
    transport.tapAt (0.0);
    transport.tapAt (0.5);
    transport.tapAt (1.0);
    transport.tapAt (1.5);

    check (nearlyEqual (transport.getTapTempoBpm(), 120.0, 1.0e-12),
           "four even taps give 120 BPM");
    check (transport.getBpm() == 120.0,
           "tap tempo writes the internal tempo");

    transport.resetTapTempo();
    transport.tapAt (0.0);
    transport.tapAt (0.48);
    transport.tapAt (1.00);
    transport.tapAt (1.48);
    const double jittered = transport.tapAt (2.00);

    check (nearlyEqual (jittered, 120.0, 1.0),
           "taps spaced 500 ms +/- 20 ms converge to 120 +/- 1 BPM");

    transport.resetTapTempo();
    transport.tapAt (0.0);
    transport.tapAt (0.5);
    const double gapReset = transport.tapAt (4.0);   // 3.5 s gap, outside range
    check (gapReset == 0.0,
           "a gap outside the supported tempo range starts a new sequence");
    check (nearlyEqual (transport.tapAt (4.5), 120.0, 1.0e-12),
           "tapping resumes after the gap");

    transport.setSyncSource (AudioEngine::Transport::SyncSource::external);
    transport.setBpm (100.0);
    transport.resetTapTempo();
    transport.tapAt (10.0);
    const double externalTap = transport.tapAt (10.5);

    check (nearlyEqual (externalTap, 120.0, 1.0e-12),
           "tapping still estimates tempo while externally synced");
    check (transport.getBpm() == 100.0,
           "tap tempo does not override an external tempo");
}

void testExternalSyncHook()
{
    std::cout << "\n-- external sync hook --" << std::endl;

    AudioEngine::Transport transport;
    transport.prepare (48000.0, 128);
    transport.setBpm (120.0);

    AudioEngine::Transport::ExternalSync update;
    update.locked = true;
    update.playing = true;
    update.bpm = 90.0;
    update.ppq = 4.0;
    update.positionSamples = 128000;

    check (! transport.applyExternalSync (update),
           "external updates are ignored while the source is internal");
    check (transport.getPpq() == 0.0 && transport.getBpm() == 120.0,
           "the ignored update changes nothing");

    transport.setSyncSource (AudioEngine::Transport::SyncSource::external);
    check (transport.applyExternalSync (update),
           "a locked update is applied in external mode");

    check (transport.isPlaying() && transport.getBpm() == 90.0,
           "the external tempo and running state are adopted");
    check (nearlyEqual (transport.getPpq(), 4.0, 1.0e-12)
               && transport.getPositionSamples() == 128000,
           "the external position is adopted");
    check (transport.isExternalClockLocked(),
           "a locked update reports the clock as locked");

    advance (transport, 24000, 128);     // 0.5 s at 90 BPM -> 0.75 quarter notes
    check (nearlyEqual (transport.getPpq(), 4.75, 1.0e-12),
           "the transport advances at the external tempo between updates");

    AudioEngine::Transport::ExternalSync lost;
    lost.locked = false;

    check (transport.applyExternalSync (lost),
           "a lost clock is accepted from the external source");
    check (! transport.isPlaying() && ! transport.isExternalClockLocked(),
           "a lost clock stops the transport and clears the lock");
    check (transport.getBpm() == 90.0 && nearlyEqual (transport.getPpq(), 4.75, 1.0e-12),
           "a lost clock holds tempo and position");

    const double held = transport.getPpq();
    advance (transport, 48000, 128);
    check (transport.getPpq() == held,
           "a stopped external transport does not advance");

    transport.setSyncSource (AudioEngine::Transport::SyncSource::internal);
    check (! transport.isExternalClockLocked(),
           "switching back to internal clears the external lock");
}

void testSnapshot()
{
    std::cout << "\n-- snapshot --" << std::endl;

    AudioEngine::Transport transport;
    transport.prepare (48000.0, 256);
    transport.setBpm (128.0);
    transport.start();
    advance (transport, 12000, 128);

    const auto snapshot = transport.snapshot();

    check (snapshot.playing, "snapshot reports the playing state");
    check (snapshot.bpm == 128.0, "snapshot reports the tempo");
    check (nearlyEqual (snapshot.ppq, 12000.0 * 128.0 / (60.0 * 48000.0), 1.0e-12),
           "snapshot reports PPQ");
    check (snapshot.positionSamples == 12000, "snapshot reports the frame position");
    check (snapshot.sampleRate == 48000.0 && snapshot.maximumBlockSize == 256,
           "snapshot reports the device format");
    check (snapshot.syncSource == AudioEngine::Transport::SyncSource::internal,
           "snapshot reports the sync source");
}

void testProcessAllocationFree()
{
    std::cout << "\n-- process() is allocation free --" << std::endl;

    AudioEngine::Transport transport;
    transport.prepare (48000.0, 128);
    transport.setBpm (120.0);
    transport.start();

    allocCount::total.store (0, std::memory_order_relaxed);
    allocCount::enabled.store (true, std::memory_order_relaxed);

    for (int block = 0; block < 10000; ++block)
        transport.process (128);

    allocCount::enabled.store (false, std::memory_order_relaxed);

    std::cout << "allocations during 10000 process() calls: "
              << allocCount::total.load (std::memory_order_relaxed) << std::endl;

    check (allocCount::total.load (std::memory_order_relaxed) == 0,
           "10000 process() calls allocate nothing");
    check (nearlyEqual (transport.getPpq(), 10000.0 * 128.0 / 24000.0, 1.0e-9),
           "the rendered position is exact after 10000 blocks");
}

} // namespace

int main()
{
    testAcceptance();
    testStartStopAndContinue();
    testRewindAndReposition();
    testTempoChangesAndRates();
    testSampleOffsetProjection();
    testTapTempo();
    testExternalSyncHook();
    testSnapshot();
    testProcessAllocationFree();

    if (failures == 0)
    {
        std::cout << "\nPASS: transport" << std::endl;
        return 0;
    }

    std::cout << "\nFAIL: " << failures << " check(s) failed" << std::endl;
    return 1;
}
