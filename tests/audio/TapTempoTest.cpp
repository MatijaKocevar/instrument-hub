// ihub-tap-tempo-test: acceptance check for T074 (tap tempo).
//
// The acceptance sentence is:
//
//     taps spaced 500 ms +/- 20 ms converge to 120 +/- 1 BPM after 4 taps.
//
// Jitter is deterministic: every tap sits within 20 ms of the 500 ms grid, and
// the median of the resulting intervals is exactly 500 ms. No wall clock is
// involved, as plan/TESTING.md requires.
//
// Also covered: convergence for several jitter patterns, a single flam not
// moving a settled tempo, the median window keeping only the last N intervals,
// sequence resets for gaps and non-increasing timestamps, the 20/300 BPM range
// boundaries, non-finite input, allocation-free tapping, and the Transport
// integration (internal tempo update, external-sync protection, snapshots).

#include "audio/TapTempo.h"
#include "audio/Transport.h"

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <string>

//==============================================================================
// Counting allocator: tapping must not allocate.

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

//==============================================================================

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

/** Records four taps and returns the estimate after the fourth. */
double fourTaps (const double* seconds)
{
    AudioEngine::TapTempo tempo;

    for (int index = 0; index < 4; ++index)
        tempo.tapAt (seconds[index]);

    return tempo.getBpm();
}

void testAcceptance()
{
    std::cout << "\n-- acceptance: 4 taps at 500 ms +/- 20 ms -> 120 +/- 1 BPM --"
              << std::endl;

    // Each tap is within 20 ms of the ideal 0/500/1000/1500 ms grid. The three
    // intervals are 500, 520 and 480 ms, so the median is exactly 500 ms.
    const double taps[] { 0.0, 0.50, 1.02, 1.50 };
    const double estimate = fourTaps (taps);

    std::cout << "     estimate after 4 taps: " << estimate
              << " BPM (expected 120 +/- 1)" << std::endl;

    check (nearlyEqual (estimate, 120.0, 1.0),
           "4 taps at 500 ms +/- 20 ms converge to 120 +/- 1 BPM");

    // Jitter on either side of the grid must not move the median.
    const double earlyMiddle[] { 0.0, 0.48, 0.98, 1.50 };   // 480, 500, 520
    const double lateMiddle[]  { 0.0, 0.52, 1.00, 1.50 };   // 520, 480, 500

    std::cout << "     early-middle pattern: " << fourTaps (earlyMiddle)
              << " BPM" << std::endl;
    std::cout << "     late-middle pattern:  " << fourTaps (lateMiddle)
              << " BPM" << std::endl;

    check (nearlyEqual (fourTaps (earlyMiddle), 120.0, 1.0),
           "taps jittered early in the middle still converge to 120 +/- 1 BPM");
    check (nearlyEqual (fourTaps (lateMiddle), 120.0, 1.0),
           "taps jittered late in the middle still converge to 120 +/- 1 BPM");

    // The estimate is available from the second tap and stays at 0 before it.
    AudioEngine::TapTempo tempo;
    check (tempo.tapAt (0.0) == 0.0, "one tap gives no estimate");
    check (tempo.getTapCount() == 1, "one tap is counted");
    check (! tempo.hasEstimate(), "hasEstimate() is false after one tap");
    check (nearlyEqual (tempo.tapAt (0.5), 120.0, 1.0e-12),
           "two taps 500 ms apart give 120 BPM");
    check (tempo.getTapCount() == 2, "two taps are counted");
    check (tempo.getIntervalCount() == 1, "one interval is in the window");
    check (tempo.hasEstimate(), "hasEstimate() is true after two taps");
}

void testMedianRobustness()
{
    std::cout << "\n-- median-of-last-N --" << std::endl;

    AudioEngine::TapTempo tempo;

    // Eight steady intervals.
    for (int index = 0; index <= 8; ++index)
        tempo.tapAt (0.5 * index);

    check (nearlyEqual (tempo.getBpm(), 120.0, 1.0e-12),
           "eight steady 500 ms intervals give exactly 120 BPM");
    check (tempo.getIntervalCount() == AudioEngine::TapTempo::defaultHistorySize,
           "the interval window fills to N");

    // One late tap creates a 520 ms interval and the following tap a 480 ms
    // interval. With eight slots, neither displaces the median.
    check (nearlyEqual (tempo.tapAt (4.52), 120.0, 1.0e-12),
           "one late tap does not move a settled 120 BPM tempo");
    check (nearlyEqual (tempo.tapAt (5.00), 120.0, 1.0e-12),
           "a late tap followed by an early one still reads 120 BPM");

    // A smaller window genuinely forgets older intervals.
    AudioEngine::TapTempo shortMemory (3);

    shortMemory.tapAt (0.0);
    shortMemory.tapAt (0.5);
    shortMemory.tapAt (1.0);
    shortMemory.tapAt (1.5);
    check (nearlyEqual (shortMemory.getBpm(), 120.0, 1.0e-12),
           "a window of 3 steady 500 ms intervals gives 120 BPM");

    shortMemory.tapAt (1.9);
    check (nearlyEqual (shortMemory.getBpm(), 120.0, 1.0e-12),
           "the first 400 ms interval is outvoted 2:1");

    shortMemory.tapAt (2.3);
    check (nearlyEqual (shortMemory.getBpm(), 150.0, 1.0e-12),
           "a 3-interval window follows a new 400 ms tempo after two taps");

    shortMemory.tapAt (2.7);
    check (nearlyEqual (shortMemory.getBpm(), 150.0, 1.0e-12),
           "the window has fully forgotten the old tempo");

    check (shortMemory.getHistorySize() == 3, "the configured history size is reported");
    check (AudioEngine::TapTempo (0).getHistorySize() == 1,
           "a zero history size is clamped to one interval");
    check (AudioEngine::TapTempo (1000).getHistorySize()
               == AudioEngine::TapTempo::maximumHistorySize,
           "an oversized history is clamped to the fixed storage");
}

void testSequenceReset()
{
    std::cout << "\n-- sequence resets --" << std::endl;

    AudioEngine::TapTempo tempo;

    tempo.tapAt (0.0);
    tempo.tapAt (0.5);
    check (tempo.getBpm() > 0.0, "a 120 BPM sequence has an estimate");

    // 3.5 s exceeds 60 / 20 = 3 s.
    check (tempo.tapAt (4.0) == 0.0, "a gap longer than 3 s starts a new sequence");
    check (tempo.getTapCount() == 1, "the new sequence has one tap");
    check (tempo.getIntervalCount() == 0, "the new sequence has no intervals");
    check (tempo.getLastIntervalSeconds() == 0.0, "the last interval is cleared");
    check (nearlyEqual (tempo.tapAt (4.5), 120.0, 1.0e-12),
           "tapping resumes after the gap");

    check (tempo.tapAt (4.5) == 0.0,
           "a repeated timestamp starts a new sequence instead of dividing by zero");
    check (tempo.tapAt (4.0) == 0.0,
           "a backwards timestamp starts a new sequence");
    check (nearlyEqual (tempo.tapAt (4.5), 120.0, 1.0e-12),
           "the backwards tap left a clean one-tap sequence");

    // Intervals shorter than 200 ms cannot be a supported tempo.
    AudioEngine::TapTempo fast;
    fast.tapAt (10.0);
    check (fast.tapAt (10.1) == 0.0,
           "an interval below 200 ms starts a new sequence");

    // Non-finite input leaves the state alone.
    AudioEngine::TapTempo robust;
    robust.tapAt (20.0);
    robust.tapAt (20.5);
    const int tapsBefore = robust.getTapCount();

    check (nearlyEqual (robust.tapAt (std::numeric_limits<double>::infinity()),
                        120.0, 1.0e-12),
           "an infinite timestamp returns the current estimate");
    check (nearlyEqual (robust.tapAt (std::numeric_limits<double>::quiet_NaN()),
                        120.0, 1.0e-12),
           "a NaN timestamp returns the current estimate");
    check (robust.getTapCount() == tapsBefore,
           "non-finite timestamps do not record a tap");
}

void testTempoRange()
{
    std::cout << "\n-- tempo range --" << std::endl;

    struct Case
    {
        double interval;
        double expectedBpm;
    };

    const Case cases[]
    {
        { 0.200, AudioEngine::TapTempo::maxBpm },
        { 0.250, 240.0 },
        { 0.333333333333333, 180.0 },
        { 0.500, 120.0 },
        { 0.666666666666667, 90.0 },
        { 1.000, 60.0 },
        { 3.000, AudioEngine::TapTempo::minBpm },
    };

    for (const auto& testCase : cases)
    {
        AudioEngine::TapTempo tempo;
        tempo.tapAt (0.0);
        const double estimate = tempo.tapAt (testCase.interval);
        const double tolerance = testCase.expectedBpm * 1.0e-9;

        check (nearlyEqual (estimate, testCase.expectedBpm, tolerance),
               "an interval of " + std::to_string (testCase.interval)
                   + " s gives " + std::to_string (testCase.expectedBpm) + " BPM");
    }

    check (nearlyEqual (AudioEngine::TapTempo::intervalSecondsForBpm (120.0),
                        0.5, 1.0e-12),
           "intervalSecondsForBpm(120) is 0.5");
    check (nearlyEqual (AudioEngine::TapTempo::bpmForIntervalSeconds (0.5),
                        120.0, 1.0e-12),
           "bpmForIntervalSeconds(0.5) is 120");
    check (AudioEngine::TapTempo::intervalSecondsForBpm (0.0) == 0.0,
           "a non-positive tempo has no interval");
    check (AudioEngine::TapTempo::bpmForIntervalSeconds (0.0) == 0.0,
           "a non-positive interval has no tempo");
}

void testReset()
{
    std::cout << "\n-- reset --" << std::endl;

    AudioEngine::TapTempo tempo;
    tempo.tapAt (0.0);
    tempo.tapAt (0.5);
    tempo.reset();

    check (tempo.getBpm() == 0.0, "reset clears the estimate");
    check (! tempo.hasEstimate(), "reset clears hasEstimate()");
    check (tempo.getTapCount() == 0, "reset clears the tap count");
    check (tempo.getIntervalCount() == 0, "reset clears the interval window");
    check (tempo.getLastIntervalSeconds() == 0.0, "reset clears the last interval");

    check (nearlyEqual (tempo.tapAt (10.0 + 0.5), 0.0, 1.0e-12),
           "the tap after reset starts a new sequence");

    // The wall-clock overload is a thin wrapper around tapAt().
    AudioEngine::TapTempo wallClock;
    check (wallClock.tap() == 0.0, "tap() needs two calls for an estimate");
    check (wallClock.getTapCount() == 1, "tap() records the first tap");
}

void testAllocationFree()
{
    std::cout << "\n-- allocation-free tapping --" << std::endl;

    AudioEngine::TapTempo tempo;

    allocCount::total.store (0, std::memory_order_relaxed);
    allocCount::enabled.store (true, std::memory_order_relaxed);

    for (int index = 0; index < 1000; ++index)
        tempo.tapAt (0.5 * index);

    allocCount::enabled.store (false, std::memory_order_relaxed);

    check (allocCount::total.load (std::memory_order_relaxed) == 0,
           "1000 taps allocate nothing");
}

void testTransportIntegration()
{
    std::cout << "\n-- transport integration --" << std::endl;

    AudioEngine::Transport transport;
    transport.prepare (48000.0, 128);
    transport.setBpm (100.0);

    const double taps[] { 0.0, 0.50, 1.02, 1.50 };
    double estimate = 0.0;

    for (double seconds : taps)
        estimate = transport.tapAt (seconds);

    check (nearlyEqual (estimate, 120.0, 1.0),
           "Transport::tapAt() converges to 120 +/- 1 BPM");
    check (nearlyEqual (transport.getBpm(), 120.0, 1.0),
           "an internal transport adopts the tapped tempo");
    check (nearlyEqual (transport.getTapTempoBpm(), 120.0, 1.0),
           "the transport snapshot carries the tap estimate");
    check (nearlyEqual (transport.snapshot().tapTempoBpm, 120.0, 1.0),
           "Transport::snapshot().tapTempoBpm matches the estimate");

    transport.resetTapTempo();
    check (transport.getTapTempoBpm() == 0.0, "resetTapTempo() clears the estimate");

    transport.setSyncSource (AudioEngine::Transport::SyncSource::external);
    transport.setBpm (100.0);
    transport.tapAt (10.0);
    const double externalEstimate = transport.tapAt (10.5);

    check (nearlyEqual (externalEstimate, 120.0, 1.0e-12),
           "tapping still estimates tempo while externally synced");
    check (transport.getBpm() == 100.0,
           "a successful tap does not override an external tempo");
}

} // namespace

int main()
{
    std::cout << "ihub-tap-tempo-test" << std::endl;

    testAcceptance();
    testMedianRobustness();
    testSequenceReset();
    testTempoRange();
    testReset();
    testAllocationFree();
    testTransportIntegration();

    if (failures == 0)
    {
        std::cout << "\nPASS: tap tempo" << std::endl;
        return 0;
    }

    std::cout << "\nFAIL: " << failures << " check(s) failed" << std::endl;
    return 1;
}
