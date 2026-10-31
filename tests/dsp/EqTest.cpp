// ihub-eq-test: acceptance check for T060 (built-in 3-band EQ).
//
// The class is a `juce::dsp::IIR` cascade: low shelf -> peak -> high shelf,
// one second-order section per band per channel. This test is deliberately
// independent of the implementation's maths:
//
// 1. An EQ with all gains at 0 dB (and a globally bypassed EQ) is a bit-exact
//    passthrough.
// 2. The magnitude response is measured by pushing a unit impulse through
//    `processBlock()` and evaluating the DFT of the resulting impulse response
//    at three test frequencies (50 Hz, 1 kHz, 10 kHz). Each measurement must be
//    within 0.5 dB of the analytic target, which the test computes from the
//    standard RBJ cookbook equations in double precision.
// 3. A coefficient update made on the message thread is applied on the next
//    block: the measured response tracks the new analytic target, and neither
//    the update application nor the processing allocates (counting operator
//    new plus wrapped malloc/calloc/realloc).
// 4. State serialisation round-trips the three bands and the global enable.
//
// The impulse/DFT method is deterministic and does not depend on wall-clock
// timing (plan/TESTING.md, "Determinism").

#include "dsp/Eq.h"

#include <juce_events/juce_events.h>

#include <atomic>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>
#include <string>
#include <vector>

//==============================================================================
// Counting allocator (same pattern as ihub-audio-engine-test / T032).

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

#if defined(__linux__) && (defined(__GNUC__) || defined(__clang__))
extern "C"
{
    void* __real_malloc (std::size_t);
    void* __wrap_malloc (std::size_t size)
    {
        allocCount::note();
        return __real_malloc (size);
    }

    void* __real_calloc (std::size_t count, std::size_t size);
    void* __wrap_calloc (std::size_t count, std::size_t size)
    {
        allocCount::note();
        return __real_calloc (count, size);
    }

    void* __real_realloc (void* pointer, std::size_t size);
    void* __wrap_realloc (void* pointer, std::size_t size)
    {
        allocCount::note();
        return __real_realloc (pointer, size);
    }
}
#endif

//==============================================================================
namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 512;
constexpr int kImpulseLength = 1 << 15; // 32768 frames, 0.68 s
constexpr double kToleranceDb = 0.5;

int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

//==============================================================================
// Independent analytic target: RBJ audio-EQ-cookbook biquads in double
// precision. Nothing here calls into ihub::dsp::Eq.

struct TargetBiquad
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0;
    double a1 = 0.0, a2 = 0.0;
};

TargetBiquad designTarget (ihub::dsp::EqBandType type,
                           double sampleRate,
                           double frequency,
                           double q,
                           double gainDb,
                           bool enabled)
{
    if (! enabled || std::abs (gainDb) < 1.0e-9)
        return {};

    const auto A = std::pow (10.0, gainDb / 40.0);
    const auto w0 = juce::MathConstants<double>::twoPi * frequency / sampleRate;
    const auto cosw0 = std::cos (w0);
    const auto sinw0 = std::sin (w0);

    double n0 = 1.0, n1 = 0.0, n2 = 0.0;
    double d0 = 1.0, d1 = 0.0, d2 = 0.0;

    if (type == ihub::dsp::EqBandType::peak)
    {
        const auto alpha = sinw0 / (2.0 * q);

        n0 = 1.0 + alpha * A;
        n1 = -2.0 * cosw0;
        n2 = 1.0 - alpha * A;

        d0 = 1.0 + alpha / A;
        d1 = -2.0 * cosw0;
        d2 = 1.0 - alpha / A;
    }
    else
    {
        // Q form of the shelf slope: beta == 2 * sqrt(A) * alpha.
        const auto beta = sinw0 * std::sqrt (A) / q;
        const auto aPlus1 = A + 1.0;
        const auto aMinus1 = A - 1.0;
        const auto aMinus1Cos = aMinus1 * cosw0;

        if (type == ihub::dsp::EqBandType::lowShelf)
        {
            n0 = A * (aPlus1 - aMinus1Cos + beta);
            n1 = 2.0 * A * (aMinus1 - aPlus1 * cosw0);
            n2 = A * (aPlus1 - aMinus1Cos - beta);

            d0 = aPlus1 + aMinus1Cos + beta;
            d1 = -2.0 * (aMinus1 + aPlus1 * cosw0);
            d2 = aPlus1 + aMinus1Cos - beta;
        }
        else // highShelf
        {
            n0 = A * (aPlus1 + aMinus1Cos + beta);
            n1 = -2.0 * A * (aMinus1 + aPlus1 * cosw0);
            n2 = A * (aPlus1 + aMinus1Cos - beta);

            d0 = aPlus1 - aMinus1Cos + beta;
            d1 = 2.0 * (aMinus1 - aPlus1 * cosw0);
            d2 = aPlus1 - aMinus1Cos - beta;
        }
    }

    TargetBiquad result;
    result.b0 = n0 / d0;
    result.b1 = n1 / d0;
    result.b2 = n2 / d0;
    result.a1 = d1 / d0;
    result.a2 = d2 / d0;
    return result;
}

double targetMagnitudeDb (const std::array<ihub::dsp::EqBandSettings,
                                           ihub::dsp::Eq::numBands>& bands,
                          double sampleRate,
                          double frequency)
{
    const auto omega = juce::MathConstants<double>::twoPi * frequency / sampleRate;
    const auto z1 = std::polar (1.0, -omega);
    const auto z2 = z1 * z1;

    std::complex<double> response (1.0, 0.0);

    for (const auto& band : bands)
    {
        const auto biquad = designTarget (band.type, sampleRate, band.frequencyHz,
                                          band.q, band.gainDb, band.enabled);

        const auto numerator = biquad.b0 + biquad.b1 * z1 + biquad.b2 * z2;
        const auto denominator = 1.0 + biquad.a1 * z1 + biquad.a2 * z2;

        response *= numerator / denominator;
    }

    return juce::Decibels::gainToDecibels (std::abs (response), -300.0);
}

//==============================================================================
/** Pushes a unit impulse through the EQ and evaluates H(e^jw) from the
    resulting impulse response. */
double measureMagnitudeDb (ihub::dsp::Eq& eq, double sampleRate, double frequency)
{
    juce::AudioBuffer<float> response (1, kImpulseLength);
    response.clear();
    response.setSample (0, 0, 1.0f);

    eq.reset();

    juce::MidiBuffer midi;
    eq.processBlock (response, midi);

    std::complex<double> sum (0.0, 0.0);
    const auto step = -juce::MathConstants<double>::twoPi * frequency / sampleRate;

    // Recurrence keeps the phase coherent without 32768 trig calls.
    const auto rotation = std::polar (1.0, step);
    std::complex<double> phase (1.0, 0.0);

    for (int n = 0; n < kImpulseLength; ++n)
    {
        sum += static_cast<double> (response.getSample (0, n)) * phase;
        phase *= rotation;
    }

    return juce::Decibels::gainToDecibels (std::abs (sum), -300.0);
}

//==============================================================================
std::array<ihub::dsp::EqBandSettings, ihub::dsp::Eq::numBands> makeTestBands()
{
    using ihub::dsp::EqBandSettings;
    using ihub::dsp::EqBandType;

    return { EqBandSettings { EqBandType::lowShelf, 100.0f, 6.0f, 0.7071068f, true },
             EqBandSettings { EqBandType::peak, 1000.0f, -4.0f, 1.0f, true },
             EqBandSettings { EqBandType::highShelf, 8000.0f, 3.0f, 0.7071068f, true } };
}

void testFlatIsBitExact()
{
    std::cout << "\n-- a flat / bypassed EQ is a bit-exact passthrough --" << std::endl;

    ihub::dsp::Eq eq;
    eq.prepareToPlay (kSampleRate, kBlockSize);

    juce::AudioBuffer<float> signal (2, kBlockSize);

    for (int channel = 0; channel < signal.getNumChannels(); ++channel)
        for (int i = 0; i < signal.getNumSamples(); ++i)
            signal.setSample (channel, i,
                              static_cast<float> (((i * 37 + channel * 11) % 101) - 50)
                                  / 50.0f);

    juce::AudioBuffer<float> reference;
    reference.makeCopyOf (signal);

    juce::MidiBuffer midi;
    eq.processBlock (signal, midi);

    bool bitEqual = true;
    const auto bytes = static_cast<std::size_t> (kBlockSize) * sizeof (float);

    for (int channel = 0; channel < signal.getNumChannels(); ++channel)
        if (std::memcmp (signal.getReadPointer (channel),
                         reference.getReadPointer (channel),
                         bytes) != 0)
            bitEqual = false;

    check (bitEqual, "0 dB on all three bands leaves the buffer bit-identical");

    eq.setEnabled (false);
    signal.makeCopyOf (reference);
    eq.processBlock (signal, midi);

    bitEqual = true;

    for (int channel = 0; channel < signal.getNumChannels(); ++channel)
        if (std::memcmp (signal.getReadPointer (channel),
                         reference.getReadPointer (channel),
                         bytes) != 0)
            bitEqual = false;

    check (bitEqual, "global bypass leaves the buffer bit-identical");

    eq.setEnabled (true);
    check (eq.isEnabled(), "the EQ can be re-enabled");
}

void testResponseMatchesAnalyticTarget()
{
    std::cout << "\n-- magnitude response vs analytic RBJ target --" << std::endl;

    ihub::dsp::Eq eq;
    eq.prepareToPlay (kSampleRate, kBlockSize);

    const auto bands = makeTestBands();
    eq.setAllBands (bands);

    check (eq.getBand (0) == bands[0], "band 0 settings are stored");
    check (eq.getBand (1) == bands[1], "band 1 settings are stored");
    check (eq.getBand (2) == bands[2], "band 2 settings are stored");

    // Three test frequencies: low-shelf region, bell centre, high-shelf region.
    constexpr std::array<double, 3> testFrequencies { 50.0, 1000.0, 10000.0 };

    for (const auto frequency : testFrequencies)
    {
        const auto expected = targetMagnitudeDb (bands, kSampleRate, frequency);
        const auto measured = measureMagnitudeDb (eq, kSampleRate, frequency);
        const auto error = std::abs (measured - expected);

        std::cout << "      " << frequency << " Hz: measured "
                  << measured << " dB, target " << expected << " dB, error "
                  << error << " dB" << std::endl;

        check (error <= kToleranceDb,
               "magnitude at " + std::to_string (static_cast<int> (frequency))
                   + " Hz is within 0.5 dB of the analytic target");
    }

    // The settings are far from unity; a dead EQ would fail the checks above,
    // but report the intended shape explicitly as well.
    check (std::abs (measureMagnitudeDb (eq, kSampleRate, 50.0)
                     - targetMagnitudeDb (bands, kSampleRate, 50.0)) <= kToleranceDb,
           "low shelf lifts the low band");
    check (measureMagnitudeDb (eq, kSampleRate, 50.0) > 3.0,
           "50 Hz actually gets a low-shelf boost");
    check (measureMagnitudeDb (eq, kSampleRate, 1000.0) < -2.0,
           "1 kHz actually gets the bell cut");
}

void testCoefficientUpdateIsRealtimeSafe()
{
    std::cout << "\n-- coefficient update is applied on the next block, RT-safe --"
              << std::endl;

    ihub::dsp::Eq eq;
    eq.prepareToPlay (kSampleRate, kBlockSize);

    auto bands = makeTestBands();
    eq.setAllBands (bands);

    const auto before = measureMagnitudeDb (eq, kSampleRate, 1000.0);
    const auto beforeTarget = targetMagnitudeDb (bands, kSampleRate, 1000.0);

    check (std::abs (before - beforeTarget) <= kToleranceDb,
           "initial bell response matches the target");

    // Message-thread parameter change. Publishing is allocation-free by
    // design, but only the audio-thread cost is part of this measurement.
    bands[1].gainDb = 9.0f;
    eq.setBand (1, bands[1]);

    juce::AudioBuffer<float> impulse (1, kImpulseLength);
    impulse.clear();
    impulse.setSample (0, 0, 1.0f);
    juce::MidiBuffer midi;

    allocCount::total.store (0, std::memory_order_relaxed);
    allocCount::enabled.store (true, std::memory_order_relaxed);

    eq.reset();
    eq.processBlock (impulse, midi);

    allocCount::enabled.store (false, std::memory_order_relaxed);
    const auto allocations = allocCount::total.load (std::memory_order_relaxed);

    check (allocations == 0,
           "applying an updated coefficient set and processing allocates nothing (got "
               + std::to_string (allocations) + ")");

    const auto after = measureMagnitudeDb (eq, kSampleRate, 1000.0);
    const auto afterTarget = targetMagnitudeDb (bands, kSampleRate, 1000.0);

    std::cout << "      update: measured " << after << " dB, target "
              << afterTarget << " dB" << std::endl;

    check (std::abs (after - afterTarget) <= kToleranceDb,
           "the updated bell response matches the new target");
    check (after - before > 10.0,
           "the update is audible (about 13 dB higher at the bell centre)");
}

void testStateRoundTrip()
{
    std::cout << "\n-- state round-trip --" << std::endl;

    ihub::dsp::Eq source;
    source.prepareToPlay (kSampleRate, kBlockSize);

    const auto bands = makeTestBands();
    source.setAllBands (bands);
    source.setEnabled (false);

    juce::MemoryBlock state;
    source.getStateInformation (state);

    ihub::dsp::Eq destination;
    destination.prepareToPlay (kSampleRate, kBlockSize);
    destination.setStateInformation (state.getData(), static_cast<int> (state.getSize()));

    check (destination.getAllBands() == bands, "all three bands survive a round-trip");
    check (! destination.isEnabled(), "the global enable survives a round-trip");
}

} // namespace

//==============================================================================
int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    testFlatIsBitExact();
    testResponseMatchesAnalyticTarget();
    testCoefficientUpdateIsRealtimeSafe();
    testStateRoundTrip();

    std::cout << (failures == 0 ? "\nPASS" : "\nFAIL") << ": " << failures
              << " failed check(s)" << std::endl;

    return failures == 0 ? 0 : 1;
}
