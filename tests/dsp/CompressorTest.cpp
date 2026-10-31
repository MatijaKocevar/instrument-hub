// ihub-compressor-test: acceptance check for T061 (feed-forward compressor).
//
// Everything here is deterministic and independent of the implementation's
// internals; the expected numbers come from the textbook static curve
//
//     gain reduction (dB) = (level - threshold) * (1 - 1/ratio)
//
// for a hard-knee compressor:
//
// 1. Static curve: steady 1 kHz tones at 3, 6 and 12 dB over threshold, for
//    ratios 2, 4 and 8, must settle to the expected gain reduction within
//    1 dB (measured as output RMS / input RMS after the attack has finished).
//    A signal below threshold must pass at unity gain.
// 2. Makeup gain is a static output gain and does not change the measured
//    gain reduction.
// 3. Reported lookahead latency equals the measured delay of an impulse,
//    at 44.1, 48 and 96 kHz and for several lookahead settings.
// 4. Bypass keeps the reported latency and is a bit-exact delayed copy.
// 5. Channels are linked: a loud left channel compresses a quiet right one.
// 6. Attack and release move in the documented direction, and a long silence
//    returns the gain reduction to zero.
// 7. A message-thread parameter update is picked up at the next block and
//    neither that block nor normal processing allocates.
// 8. State round-trips.

#include "dsp/Compressor.h"

#include <juce_events/juce_events.h>

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>
#include <string>
#include <vector>

//==============================================================================
// Counting allocator (same pattern as ihub-eq-test / ihub-audio-engine-test).

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
constexpr double kStaticToleranceDb = 1.0; // the acceptance tolerance
constexpr double kToneFrequency = 1000.0;

int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

/** Short, trimmed rendering for log lines (2.000000 -> 2). */
std::string number (double value)
{
    juce::String text (value, 3);
    text = text.trimCharactersAtEnd ("0");
    text = text.trimCharactersAtEnd (".");

    return text.toStdString();
}

//==============================================================================
// Signal helpers.

double amplitudeForDb (double levelDb)
{
    return std::pow (10.0, levelDb / 20.0);
}

void fillTone (juce::AudioBuffer<float>& buffer, int channel,
               double levelDb, double frequency, double sampleRate)
{
    const auto amplitude = static_cast<float> (amplitudeForDb (levelDb));

    for (int i = 0; i < buffer.getNumSamples(); ++i)
        buffer.setSample (channel, i,
                          amplitude
                              * static_cast<float> (std::sin (
                                  juce::MathConstants<double>::twoPi * frequency
                                  * static_cast<double> (i) / sampleRate)));
}

/** Renders in place in `blockSize`-sized chunks, the way a host would. */
void processInBlocks (ihub::dsp::Compressor& compressor,
                      juce::AudioBuffer<float>& buffer,
                      int blockSize)
{
    juce::MidiBuffer midi;

    for (int position = 0; position < buffer.getNumSamples(); position += blockSize)
    {
        const auto numSamples =
            juce::jmin (blockSize, buffer.getNumSamples() - position);

        juce::AudioBuffer<float> block (buffer.getArrayOfWritePointers(),
                                        buffer.getNumChannels(),
                                        position,
                                        numSamples);

        compressor.processBlock (block, midi);
    }
}

double rmsDb (const juce::AudioBuffer<float>& buffer, int channel, int from, int to)
{
    double sum = 0.0;

    for (int i = from; i < to; ++i)
    {
        const auto value = static_cast<double> (buffer.getSample (channel, i));
        sum += value * value;
    }

    return juce::Decibels::gainToDecibels (std::sqrt (sum / static_cast<double> (to - from)),
                                           -300.0);
}

/** Measures the gain (output dB - input dB) applied to a steady tone over the
    last 0.5 s of the render. Negative means gain reduction. */
double measureSteadyGainDb (ihub::dsp::Compressor& compressor,
                            double levelDb,
                            double frequency = kToneFrequency,
                            double sampleRate = kSampleRate,
                            double seconds = 1.5)
{
    const auto numSamples = static_cast<int> (seconds * sampleRate);

    juce::AudioBuffer<float> signal (1, numSamples);
    fillTone (signal, 0, levelDb, frequency, sampleRate);

    juce::AudioBuffer<float> reference;
    reference.makeCopyOf (signal);

    processInBlocks (compressor, signal, kBlockSize);

    const auto from = static_cast<int> (0.6 * sampleRate);
    const auto inputDb = rmsDb (reference, 0, from, numSamples);
    const auto outputDb = rmsDb (signal, 0, from, numSamples);

    return outputDb - inputDb;
}

//==============================================================================
// 1 + 2: static curve, makeup.

void testStaticCurve()
{
    std::cout << "\n-- static curve: 6 dB over threshold --" << std::endl;

    constexpr double thresholdDb = -18.0;

    struct Case { double ratio; double overDb; };
    const std::array<Case, 6> cases {
        Case { 2.0, 3.0 }, Case { 2.0, 6.0 },
        Case { 4.0, 3.0 }, Case { 4.0, 6.0 },
        Case { 4.0, 12.0 }, Case { 8.0, 6.0 }
    };

    for (const auto& testCase : cases)
    {
        ihub::dsp::Compressor compressor;
        compressor.prepareToPlay (kSampleRate, kBlockSize);

        ihub::dsp::CompressorSettings settings;
        settings.thresholdDb = static_cast<float> (thresholdDb);
        settings.ratio = static_cast<float> (testCase.ratio);
        settings.attackMs = 10.0f;
        settings.releaseMs = 100.0f;
        settings.makeupDb = 0.0f;
        compressor.setSettings (settings);

        const auto inputDb = thresholdDb + testCase.overDb;
        const auto measuredGainDb = measureSteadyGainDb (compressor, inputDb);
        const auto expectedGrDb = testCase.overDb * (1.0 - 1.0 / testCase.ratio);
        const auto measuredGrDb = -measuredGainDb;
        const auto error = std::abs (measuredGrDb - expectedGrDb);

        std::cout << "      ratio " << number (testCase.ratio) << ":1, "
                  << number (testCase.overDb) << " dB over: expected "
                  << number (expectedGrDb) << " dB, measured " << number (measuredGrDb)
                  << " dB, error " << number (error) << " dB" << std::endl;

        check (error <= kStaticToleranceDb,
               "ratio " + number (testCase.ratio) + ":1 at " + number (testCase.overDb)
                   + " dB over threshold reduces gain by the static curve within 1 dB");

        check (std::abs (compressor.getGainReductionDb() - expectedGrDb) <= kStaticToleranceDb,
               "the reported gain reduction matches the static curve within 1 dB");
    }

    // The literal acceptance sentence: 6 dB over threshold with the default
    // 4:1 ratio means 4.5 dB of gain reduction.
    ihub::dsp::Compressor compressor;
    compressor.prepareToPlay (kSampleRate, kBlockSize);

    const auto gainDb = measureSteadyGainDb (compressor, thresholdDb + 6.0);
    const auto measuredGr = -gainDb;

    std::cout << "      acceptance: 6 dB over, 4:1 -> " << measuredGr
              << " dB (expected 4.5 dB)" << std::endl;

    check (std::abs (measuredGr - 4.5) <= 1.0,
           "6 dB over threshold yields 4.5 dB of gain reduction within 1 dB");

    // Below threshold: unity gain, no gain reduction.
    const auto belowGainDb = measureSteadyGainDb (compressor, thresholdDb - 6.0);
    check (std::abs (belowGainDb) <= 0.01,
           "a signal below threshold passes at unity gain");
    check (std::abs (compressor.getGainReductionDb()) <= 0.01,
           "the reported gain reduction is 0 below threshold");

    // Ratio 1:1 is a no-op.
    ihub::dsp::CompressorSettings unity;
    unity.ratio = 1.0f;
    compressor.setSettings (unity);

    const auto unityGainDb = measureSteadyGainDb (compressor, thresholdDb + 12.0);
    check (std::abs (unityGainDb) <= 0.01,
           "ratio 1:1 applies no gain reduction");
}

void testMakeupGain()
{
    std::cout << "\n-- makeup gain --" << std::endl;

    ihub::dsp::Compressor compressor;
    compressor.prepareToPlay (kSampleRate, kBlockSize);

    ihub::dsp::CompressorSettings settings;
    settings.thresholdDb = -18.0f;
    settings.ratio = 4.0f;
    settings.makeupDb = 6.0f;
    compressor.setSettings (settings);

    // Below threshold: the output is the input + makeup.
    const auto lowGainDb = measureSteadyGainDb (compressor, -30.0);
    std::cout << "      below threshold: " << lowGainDb << " dB (expected 6 dB)"
              << std::endl;
    check (std::abs (lowGainDb - 6.0) <= 0.01,
           "makeup applies exactly +6 dB below threshold");

    // Above threshold: the static curve is unchanged, makeup is on top of it.
    const auto highGainDb = measureSteadyGainDb (compressor, -12.0);
    const auto highGrDb = -(highGainDb - 6.0);
    std::cout << "      6 dB over: " << highGainDb << " dB net, " << highGrDb
              << " dB of gain reduction" << std::endl;
    check (std::abs (highGrDb - 4.5) <= 1.0,
           "makeup does not change the 4.5 dB gain reduction above threshold");

    // Negative makeup is just a cut.
    settings.makeupDb = -3.0f;
    compressor.setSettings (settings);
    const auto cutGainDb = measureSteadyGainDb (compressor, -30.0);
    check (std::abs (cutGainDb + 3.0) <= 0.01,
           "makeup applies exactly -3 dB below threshold");
}

//==============================================================================
// 3: reported latency equals measured delay.

int measureImpulseDelay (ihub::dsp::Compressor& compressor,
                         int impulseIndex)
{
    const auto numSamples = impulseIndex + 4096;

    juce::AudioBuffer<float> buffer (1, numSamples);
    buffer.clear();
    buffer.setSample (0, impulseIndex, 1.0f);

    processInBlocks (compressor, buffer, kBlockSize);

    int peakIndex = 0;
    float peak = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto magnitude = std::abs (buffer.getSample (0, i));

        if (magnitude > peak)
        {
            peak = magnitude;
            peakIndex = i;
        }
    }

    // Only the impulse may be non-zero; anything else means the delay line
    // is leaking or the gain stage is ringing.
    for (int i = 0; i < numSamples; ++i)
        if (i != peakIndex && std::abs (buffer.getSample (0, i)) > 1.0e-6f)
            return -1000 - i;

    if (peak <= 0.0f)
        return -1;

    return peakIndex - impulseIndex;
}

void testLatency()
{
    std::cout << "\n-- reported latency vs measured impulse delay --" << std::endl;

    constexpr double sampleRates[] { 44100.0, 48000.0, 96000.0 };
    constexpr float lookaheads[] { 0.0f, 1.0f, 2.5f, 5.0f, 10.0f, 20.0f };

    for (const auto sampleRate : sampleRates)
    {
        for (const auto lookahead : lookaheads)
        {
            ihub::dsp::Compressor compressor;
            compressor.setLookaheadMs (lookahead);
            compressor.prepareToPlay (sampleRate, kBlockSize);

            const auto expected =
                static_cast<int> (std::lround (static_cast<double> (lookahead)
                                               * 0.001 * sampleRate));

            const auto measured = measureImpulseDelay (compressor, 100);

            const auto label = number (lookahead) + " ms at "
                               + std::to_string (static_cast<int> (sampleRate)) + " Hz";

            check (compressor.getLookaheadSamples() == expected,
                   "lookahead samples for " + label + " are "
                       + std::to_string (expected));
            check (compressor.getLatencySamples() == expected,
                   "reported latency for " + label + " is " + std::to_string (expected)
                       + " samples");
            check (measured == expected,
                   "measured delay for " + label + " is " + std::to_string (measured)
                       + " samples");
        }
    }

    // A stereo impulse is delayed on both channels by the same amount.
    ihub::dsp::Compressor compressor;
    compressor.setLookaheadMs (5.0f);
    compressor.prepareToPlay (kSampleRate, kBlockSize);

    juce::AudioBuffer<float> stereo (2, 8192);
    stereo.clear();
    stereo.setSample (0, 100, 0.5f);
    stereo.setSample (1, 100, -0.25f);
    processInBlocks (compressor, stereo, kBlockSize);

    const auto expected = compressor.getLatencySamples();
    check (std::abs (stereo.getSample (0, 100 + expected)) > 0.05f,
           "left impulse arrives at the reported latency");
    check (std::abs (stereo.getSample (1, 100 + expected)) > 0.02f,
           "right impulse arrives at the same reported latency");
    check (compressor.getLatencySamples() == 240,
           "5 ms at 48 kHz reports 240 samples");
}

//==============================================================================
// 4: bypass keeps latency and is a delayed pass-through.

void testBypass()
{
    std::cout << "\n-- bypass keeps the reported latency --" << std::endl;

    constexpr int numSamples = 4096;
    constexpr int impulseIndex = 64;

    ihub::dsp::Compressor compressor;
    compressor.setLookaheadMs (5.0f);
    compressor.prepareToPlay (kSampleRate, kBlockSize);
    compressor.setSettings (ihub::dsp::CompressorSettings {});

    const auto latency = compressor.getLatencySamples();

    juce::AudioBuffer<float> signal (1, numSamples);

    for (int i = 0; i < numSamples; ++i)
        signal.setSample (0, i,
                          static_cast<float> (((i * 37) % 101) - 50) / 50.0f);

    signal.setSample (0, impulseIndex, 1.0f);

    juce::AudioBuffer<float> reference;
    reference.makeCopyOf (signal);

    compressor.setEnabled (false);
    processInBlocks (compressor, signal, kBlockSize);

    bool exact = true;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto expected = i >= latency ? reference.getSample (0, i - latency) : 0.0f;

        if (! juce::exactlyEqual (signal.getSample (0, i), expected))
            exact = false;
    }

    check (exact, "bypass is a bit-exact copy delayed by the reported lookahead");
    check (compressor.getLatencySamples() == latency && latency == 240,
           "bypass does not change the reported latency");
    check (std::abs (compressor.getGainReductionDb()) < 1.0e-9f,
           "bypass reports zero gain reduction");

    // Re-enabling compresses again.
    compressor.setEnabled (true);
    compressor.reset();

    const auto gainDb = measureSteadyGainDb (compressor, -12.0);
    check (std::abs (-gainDb - 4.5) <= 1.0,
           "re-enabling restores compression");
}

//==============================================================================
// 5: stereo linking.

void testStereoLinking()
{
    std::cout << "\n-- stereo linking --" << std::endl;

    ihub::dsp::Compressor compressor;
    compressor.prepareToPlay (kSampleRate, kBlockSize);

    const auto numSamples = static_cast<int> (1.5 * kSampleRate);
    juce::AudioBuffer<float> signal (2, numSamples);

    // Left is 6 dB over the -18 dBFS threshold, right is 24 dB below it.
    fillTone (signal, 0, -12.0, kToneFrequency, kSampleRate);
    fillTone (signal, 1, -42.0, 500.0, kSampleRate);

    juce::AudioBuffer<float> reference;
    reference.makeCopyOf (signal);

    processInBlocks (compressor, signal, kBlockSize);

    const auto from = static_cast<int> (0.75 * kSampleRate);
    const auto leftGain = rmsDb (signal, 0, from, numSamples) - rmsDb (reference, 0, from, numSamples);
    const auto rightGain = rmsDb (signal, 1, from, numSamples) - rmsDb (reference, 1, from, numSamples);

    std::cout << "      left " << leftGain << " dB, right " << rightGain << " dB"
              << std::endl;

    check (std::abs (leftGain - rightGain) <= 0.05,
           "both channels receive the same gain reduction");
    check (std::abs (-leftGain - 4.5) <= 1.0,
           "the linked pair follows the loud channel's static curve");
}

//==============================================================================
// 6: attack/release direction and settling.

void testDynamics()
{
    std::cout << "\n-- attack and release movement --" << std::endl;

    const auto renderAndReadGr = [] (float attackMs, float releaseMs,
                                     double toneSeconds, double silenceSeconds,
                                     float thresholdDb)
    {
        ihub::dsp::Compressor compressor;
        compressor.prepareToPlay (kSampleRate, kBlockSize);

        ihub::dsp::CompressorSettings settings;
        settings.thresholdDb = thresholdDb;
        settings.ratio = 4.0f;
        settings.attackMs = attackMs;
        settings.releaseMs = releaseMs;
        compressor.setSettings (settings);

        const auto toneSamples = static_cast<int> (toneSeconds * kSampleRate);
        const auto silenceSamples = static_cast<int> (silenceSeconds * kSampleRate);

        juce::AudioBuffer<float> buffer (1, toneSamples + silenceSamples);
        buffer.clear();
        fillTone (buffer, 0, 0.0, kToneFrequency, kSampleRate);

        for (int i = toneSamples; i < buffer.getNumSamples(); ++i)
            buffer.setSample (0, i, 0.0f);

        processInBlocks (compressor, buffer, kBlockSize);

        return compressor.getGainReductionDb();
    };

    // Fast attack reaches more of the target in 3 ms than a slow one.
    const auto fastAttack = renderAndReadGr (1.0f, 100.0f, 0.003, 0.0, -18.0f);
    const auto slowAttack = renderAndReadGr (100.0f, 100.0f, 0.003, 0.0, -18.0f);

    std::cout << "      after 3 ms: attack 1 ms -> " << fastAttack
              << " dB, attack 100 ms -> " << slowAttack << " dB" << std::endl;

    check (fastAttack > slowAttack + 5.0,
           "a fast attack reduces gain sooner than a slow attack");
    check (fastAttack > 8.0,
           "a 1 ms attack is most of the way to the static curve after 3 ms");

    // A short release gives gain back sooner than a long one.
    const auto shortRelease = renderAndReadGr (1.0f, 50.0f, 0.2, 0.1, -18.0f);
    const auto longRelease = renderAndReadGr (1.0f, 2000.0f, 0.2, 0.1, -18.0f);

    std::cout << "      after a 100 ms gap: release 50 ms -> " << shortRelease
              << " dB, release 2000 ms -> " << longRelease << " dB" << std::endl;

    check (shortRelease < longRelease - 1.0,
           "a short release gives gain back sooner than a long release");

    // After a long silence the gain reduction is back to zero.
    const auto settled = renderAndReadGr (1.0f, 100.0f, 0.2, 1.5, -18.0f);
    check (settled <= 0.05f,
           "a long silence returns the gain reduction to 0");
}

//==============================================================================
// 7: parameter update is applied on the next block, RT-safe.

void testParameterUpdateIsRealtimeSafe()
{
    std::cout << "\n-- parameter update applied at the next block, RT-safe --"
              << std::endl;

    ihub::dsp::Compressor compressor;
    compressor.prepareToPlay (kSampleRate, kBlockSize);

    ihub::dsp::CompressorSettings settings;
    settings.thresholdDb = -18.0f;
    settings.ratio = 4.0f;
    settings.attackMs = 1.0f;
    settings.releaseMs = 50.0f;
    compressor.setSettings (settings);

    // Warm up on a tone 6 dB over the old threshold: 4.5 dB of reduction.
    juce::AudioBuffer<float> warmup (1, static_cast<int> (kSampleRate));
    fillTone (warmup, 0, -12.0, kToneFrequency, kSampleRate);
    processInBlocks (compressor, warmup, kBlockSize);

    check (std::abs (compressor.getGainReductionDb() - 4.5f) <= 1.0f,
           "warm-up reaches the old static curve");

    // New threshold: the tone is now 18 dB over, so the target is 13.5 dB.
    compressor.setThresholdDb (-30.0f);

    juce::AudioBuffer<float> block (1, kBlockSize);
    fillTone (block, 0, -12.0, kToneFrequency, kSampleRate);

    juce::MidiBuffer midi;

    allocCount::total.store (0, std::memory_order_relaxed);
    allocCount::enabled.store (true, std::memory_order_relaxed);
    compressor.processBlock (block, midi);
    allocCount::enabled.store (false, std::memory_order_relaxed);

    const auto allocations = allocCount::total.load (std::memory_order_relaxed);

    check (allocations == 0,
           "applying an update and processing a block allocates nothing (got "
               + std::to_string (allocations) + ")");

    // Keep going for 0.5 s so the new target is fully reached.
    juce::AudioBuffer<float> settle (1, static_cast<int> (0.5 * kSampleRate));
    fillTone (settle, 0, -12.0, kToneFrequency, kSampleRate);
    processInBlocks (compressor, settle, kBlockSize);

    const auto settledGr = compressor.getGainReductionDb();
    std::cout << "      new threshold -> " << settledGr << " dB (expected 13.5 dB)"
              << std::endl;

    check (std::abs (settledGr - 13.5f) <= 1.0f,
           "the update is audible on the next block and reaches its static curve");
}

//==============================================================================
// 8: state round-trip.

void testStateRoundTrip()
{
    std::cout << "\n-- state round-trip --" << std::endl;

    ihub::dsp::Compressor source;
    source.prepareToPlay (kSampleRate, kBlockSize);

    ihub::dsp::CompressorSettings settings;
    settings.thresholdDb = -24.5f;
    settings.ratio = 6.5f;
    settings.attackMs = 3.25f;
    settings.releaseMs = 250.0f;
    settings.makeupDb = 4.0f;

    source.setSettings (settings);
    source.setEnabled (false);
    source.setLookaheadMs (12.0f);

    juce::MemoryBlock state;
    source.getStateInformation (state);

    ihub::dsp::Compressor destination;
    destination.prepareToPlay (kSampleRate, kBlockSize);
    destination.setStateInformation (state.getData(),
                                     static_cast<int> (state.getSize()));

    check (destination.getSettings() == settings, "settings survive a round-trip");
    check (! destination.isEnabled(), "bypass survives a round-trip");
    check (std::abs (destination.getLookaheadMs() - 12.0f) <= 0.001f,
           "the lookahead setting survives a round-trip");
}

} // namespace

//==============================================================================
int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    testStaticCurve();
    testMakeupGain();
    testLatency();
    testBypass();
    testStereoLinking();
    testDynamics();
    testParameterUpdateIsRealtimeSafe();
    testStateRoundTrip();

    std::cout << (failures == 0 ? "\nPASS" : "\nFAIL") << ": " << failures
              << " failed check(s)" << std::endl;

    return failures == 0 ? 0 : 1;
}
