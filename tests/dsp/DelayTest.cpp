// ihub-delay-test: acceptance check for T062 (tempo-syncable delay).
//
// The acceptance sentence is:
//
//     a 1/4-note delay at 120 BPM places the echo at exactly 12000 samples
//     at 48 kHz.
//
// Everything else here checks the properties that make that number useful:
//
// 1. The pure sync function and an actual rendered impulse agree, for several
//    tempos, sample rates and note divisions. A 1/4-note delay at 120 BPM is
//    exactly 12000 samples at 48 kHz; the echo is the only non-zero output.
// 2. Free (unsynced) time in milliseconds maps to samples the same way.
// 3. Feedback produces repeats spaced by the delay time and decaying in
//    amplitude; zero feedback produces a single repeat.
// 4. A transport BPM change is picked up at a block boundary and the next
//    impulse is delayed by the new time exactly.
// 5. The transport interface accepts BPM and PPQ: the next repeat position is
//    reported in PPQ units on the transport timeline.
// 6. Bypass and a 0% mix are bit-exact dry passthroughs.
// 7. Parameter changes and normal processing allocate nothing on the audio
//    thread.
// 8. State round-trips.

#include "dsp/Delay.h"

#include <juce_events/juce_events.h>

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
// Counting allocator (same pattern as ihub-compressor-test / ihub-eq-test).

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
constexpr int kBlockSize = 256;

int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

std::string number (double value)
{
    juce::String text (value, 3);
    text = text.trimCharactersAtEnd ("0");
    text = text.trimCharactersAtEnd (".");

    return text.toStdString();
}

//==============================================================================
// Rendering helpers.

void processInBlocks (ihub::dsp::Delay& delay,
                      juce::AudioBuffer<float>& buffer,
                      int blockSize = kBlockSize)
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

        delay.processBlock (block, midi);
    }
}

/** A single unit impulse in an otherwise silent buffer. */
juce::AudioBuffer<float> makeImpulse (int numChannels, int numSamples, int impulseIndex)
{
    juce::AudioBuffer<float> buffer (numChannels, numSamples);
    buffer.clear();

    for (int channel = 0; channel < numChannels; ++channel)
        buffer.setSample (channel, impulseIndex, 1.0f);

    return buffer;
}

/** Index of the largest absolute sample, or -1 for silence. */
int peakIndex (const juce::AudioBuffer<float>& buffer, int channel,
               int from, int to)
{
    int index = -1;
    float peak = 0.0f;

    for (int i = from; i < to; ++i)
    {
        const auto magnitude = std::abs (buffer.getSample (channel, i));

        if (magnitude > peak)
        {
            peak = magnitude;
            index = i;
        }
    }

    return index;
}

/** Number of samples above `threshold`, useful for spotting leaks. */
int countNonZero (const juce::AudioBuffer<float>& buffer, int channel,
                  int from, int to, float threshold = 1.0e-6f)
{
    int count = 0;

    for (int i = from; i < to; ++i)
        if (std::abs (buffer.getSample (channel, i)) > threshold)
            ++count;

    return count;
}

void configure (ihub::dsp::Delay& delay,
                ihub::dsp::DelayDivision division,
                double bpm,
                float feedback = 0.0f,
                float mix = 1.0f)
{
    ihub::dsp::DelaySettings settings;
    settings.tempoSync = true;
    settings.division = division;
    settings.feedback = feedback;
    settings.mix = mix;
    settings.dampingHz = 20000.0f;

    delay.setSettings (settings);

    ihub::dsp::DelayTransport transport;
    transport.playing = true;
    transport.bpm = bpm;
    transport.ppq = 0.0;
    delay.setTransport (transport);
}

/** Renders one impulse and returns the echo distance in samples (-1 if it
    could not be found). */
int measureEcho (ihub::dsp::Delay& delay, int impulseIndex, int numSamples)
{
    auto buffer = makeImpulse (1, numSamples, impulseIndex);
    processInBlocks (delay, buffer);

    const auto peak = peakIndex (buffer, 0, 0, numSamples);

    if (peak < 0)
        return -1;

    return peak - impulseIndex;
}

//==============================================================================
// 1: the acceptance check.

void testAcceptance()
{
    std::cout << "\n-- acceptance: 1/4 note at 120 BPM, 48 kHz --" << std::endl;

    const auto expected = ihub::dsp::Delay::delaySamplesForDivision (
        ihub::dsp::DelayDivision::quarter, 120.0, 48000.0);

    std::cout << "      pure function: " << expected << " samples" << std::endl;
    check (expected == 12000,
           "Delay::delaySamplesForDivision(quarter, 120 BPM, 48 kHz) is 12000");

    ihub::dsp::Delay delay;
    delay.prepareToPlay (48000.0, kBlockSize);
    configure (delay, ihub::dsp::DelayDivision::quarter, 120.0);

    std::cout << "      reported delay: " << delay.getDelaySamples()
              << " samples (" << number (delay.getDelaySeconds() * 1000.0)
              << " ms)" << std::endl;

    check (delay.getDelaySamples() == 12000,
           "the prepared delay reports 12000 samples");

    constexpr int impulseIndex = 100;
    const auto numSamples = impulseIndex + 12000 + 512;

    auto buffer = makeImpulse (1, numSamples, impulseIndex);
    processInBlocks (delay, buffer);

    const auto echo = peakIndex (buffer, 0, 0, numSamples);
    const auto echoLevel = buffer.getSample (0, echo);

    std::cout << "      impulse at " << impulseIndex << ", echo at " << echo
              << " (distance " << echo - impulseIndex << "), level " << echoLevel
              << std::endl;

    check (echo == impulseIndex + 12000,
           "the echo lands at exactly input + 12000 samples");
    check (std::abs (echoLevel - 1.0f) < 1.0e-6f,
           "the echo passes at unity gain (100% wet, no feedback)");
    check (countNonZero (buffer, 0, 0, numSamples) == 1,
           "only the echo is non-zero: no leak before or after the repeat");
    check (delay.getLatencySamples() == 0,
           "a delay reports zero latency (the dry signal is not delayed)");
}

//==============================================================================
// 2 + 3: sync table, free time, feedback.

void testDivisionTable()
{
    std::cout << "\n-- sync divisions and sample rates --" << std::endl;

    using D = ihub::dsp::DelayDivision;

    struct Case
    {
        D division;
        double bpm;
        double sampleRate;
        int expected;
    };

    const Case cases[] {
        { D::quarter,        120.0, 48000.0, 12000 },
        { D::quarter,         90.0, 48000.0, 16000 },
        { D::quarter,         60.0, 48000.0, 24000 },
        { D::quarter,        120.0, 44100.0, 11025 },
        { D::eighth,         120.0, 48000.0,  6000 },
        { D::half,           120.0, 48000.0, 24000 },
        { D::whole,          120.0, 48000.0, 48000 },
        { D::sixteenth,      120.0, 48000.0,  3000 },
        { D::dottedQuarter,  120.0, 48000.0, 18000 },
        { D::dottedEighth,   120.0, 48000.0,  9000 },
        { D::tripletQuarter, 120.0, 48000.0,  8000 },
        { D::tripletEighth,  120.0, 48000.0,  4000 },
    };

    for (const auto& testCase : cases)
    {
        const auto measured = ihub::dsp::Delay::delaySamplesForDivision (
            testCase.division, testCase.bpm, testCase.sampleRate);

        const auto label = "division " + std::to_string (static_cast<int> (testCase.division))
                           + " at " + number (testCase.bpm) + " BPM, "
                           + std::to_string (static_cast<int> (testCase.sampleRate)) + " Hz";

        check (measured == testCase.expected,
               label + " is " + std::to_string (testCase.expected)
                   + " samples (got " + std::to_string (measured) + ")");
    }

    // The same numbers must come out of a rendered impulse, not just the
    // pure function.
    ihub::dsp::Delay delay;
    delay.prepareToPlay (44100.0, kBlockSize);
    configure (delay, D::quarter, 120.0);

    const auto echo = measureEcho (delay, 64, 64 + 11025 + 256);
    std::cout << "      rendered 1/4 at 120 BPM / 44.1 kHz: " << echo << " samples"
              << std::endl;
    check (echo == 11025, "a rendered echo at 44.1 kHz is 11025 samples away");
}

void testFreeTime()
{
    std::cout << "\n-- free (unsynced) time --" << std::endl;

    check (ihub::dsp::Delay::delaySamplesForMs (250.0f, 48000.0) == 12000,
           "250 ms at 48 kHz is 12000 samples");

    ihub::dsp::Delay delay;
    delay.prepareToPlay (48000.0, kBlockSize);

    ihub::dsp::DelaySettings settings;
    settings.tempoSync = false;
    settings.timeMs = 120.0f;
    settings.feedback = 0.0f;
    settings.mix = 1.0f;
    settings.dampingHz = 20000.0f;
    delay.setSettings (settings);

    const auto echo = measureEcho (delay, 32, 32 + 5760 + 256);

    std::cout << "      120 ms at 48 kHz: " << echo << " samples (expected 5760)"
              << std::endl;
    check (echo == 5760, "a 120 ms free delay renders at 5760 samples");
}

void testFeedback()
{
    std::cout << "\n-- feedback repeats --" << std::endl;

    ihub::dsp::Delay delay;
    delay.prepareToPlay (48000.0, kBlockSize);
    configure (delay, ihub::dsp::DelayDivision::quarter, 120.0, 0.5f, 1.0f);

    constexpr int impulseIndex = 16;
    constexpr int delaySamples = 12000;
    const auto numSamples = impulseIndex + 3 * delaySamples + 64;

    auto buffer = makeImpulse (1, numSamples, impulseIndex);
    processInBlocks (delay, buffer);

    const auto first = peakIndex (buffer, 0, 0, numSamples);
    const auto second = peakIndex (buffer, 0, first + 1, numSamples);
    const auto third = peakIndex (buffer, 0, second + 1, numSamples);

    std::cout << "      peaks: " << first << ", " << second << ", " << third
              << std::endl;

    check (first == impulseIndex + delaySamples, "first repeat is one delay later");
    check (second == first + delaySamples, "second repeat is two delays later");
    check (third == second + delaySamples, "third repeat is three delays later");

    const auto level1 = std::abs (buffer.getSample (0, first));
    const auto level2 = std::abs (buffer.getSample (0, second));
    const auto level3 = std::abs (buffer.getSample (0, third));

    std::cout << "      levels: " << level1 << ", " << level2 << ", " << level3
              << std::endl;

    check (level2 < level1 && level3 < level2,
           "successive repeats decay with feedback");
}

//==============================================================================
// 4 + 5: transport.

void testTransportChange()
{
    std::cout << "\n-- transport BPM change --" << std::endl;

    ihub::dsp::Delay delay;
    delay.prepareToPlay (48000.0, kBlockSize);
    configure (delay, ihub::dsp::DelayDivision::quarter, 120.0);

    // Let the delay run for a while, then change the tempo. The change is
    // adopted at the next block and the read tap crossfades to the new time.
    juce::AudioBuffer<float> warmUp (1, 2000);
    warmUp.clear();
    processInBlocks (delay, warmUp);

    ihub::dsp::DelayTransport transport;
    transport.playing = true;
    transport.bpm = 60.0;
    delay.setTransport (transport);

    check (delay.getDelaySamples() == 24000,
           "a 60 BPM quarter note reports 24000 samples");

    juce::AudioBuffer<float> settle (1, 4000);
    settle.clear();
    processInBlocks (delay, settle);

    const auto echo = measureEcho (delay, 0, 24000 + 512);

    std::cout << "      after changing to 60 BPM the echo is " << echo
              << " samples away" << std::endl;
    check (echo == 24000, "the impulse after the change repeats at 24000 samples");

    // The transport's PPQ position is accepted and used for the reported
    // musical position of the next repeat.
    transport.ppq = 3.0;
    delay.setTransport (transport);

    check (juce::exactlyEqual (delay.getTransport().ppq, 3.0),
           "the transport PPQ is stored");
    check (std::abs (delay.getDelayInPpq() - 0.5) < 1.0e-12,
           "at 60 BPM a 24000-sample delay is 0.5 PPQ (half a quarter note)");
    check (std::abs (delay.getNextRepeatPpq() - 3.5) < 1.0e-12,
           "the next repeat lands at PPQ 3.5 from PPQ 3.0");
}

//==============================================================================
// 6: bypass and dry mix.

void testBypassAndDryMix()
{
    std::cout << "\n-- bypass and 0% mix --" << std::endl;

    ihub::dsp::Delay delay;
    delay.prepareToPlay (48000.0, kBlockSize);
    configure (delay, ihub::dsp::DelayDivision::quarter, 120.0, 0.5f, 0.0f);

    juce::AudioBuffer<float> signal (2, 4096);

    for (int channel = 0; channel < signal.getNumChannels(); ++channel)
        for (int i = 0; i < signal.getNumSamples(); ++i)
            signal.setSample (channel, i,
                              std::sin (0.01f * static_cast<float> (i + channel)));

    juce::AudioBuffer<float> reference;
    reference.makeCopyOf (signal);

    processInBlocks (delay, signal);

    bool dryExact = true;

    for (int channel = 0; channel < signal.getNumChannels(); ++channel)
        for (int i = 0; i < signal.getNumSamples(); ++i)
            if (! juce::exactlyEqual (signal.getSample (channel, i),
                                      reference.getSample (channel, i)))
                dryExact = false;

    check (dryExact, "a 0% mix is a bit-exact dry passthrough");

    delay.setEnabled (false);
    signal.makeCopyOf (reference);
    processInBlocks (delay, signal);

    dryExact = true;

    for (int channel = 0; channel < signal.getNumChannels(); ++channel)
        for (int i = 0; i < signal.getNumSamples(); ++i)
            if (! juce::exactlyEqual (signal.getSample (channel, i),
                                      reference.getSample (channel, i)))
                dryExact = false;

    check (dryExact, "bypass is a bit-exact dry passthrough");

    delay.setEnabled (true);
}

//==============================================================================
// 7: realtime safety.

void testParameterUpdateIsRealtimeSafe()
{
    std::cout << "\n-- realtime safety --" << std::endl;

    ihub::dsp::Delay delay;
    delay.prepareToPlay (48000.0, kBlockSize);
    configure (delay, ihub::dsp::DelayDivision::quarter, 120.0, 0.3f, 1.0f);

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    juce::MidiBuffer midi;

    const auto processOneBlock = [&]
    {
        buffer.clear();
        delay.processBlock (buffer, midi);
    };

    // Warm up outside the counter (JUCE can lazily set up denormal state on
    // the first call).
    for (int i = 0; i < 4; ++i)
        processOneBlock();

    allocCount::total.store (0, std::memory_order_relaxed);
    allocCount::enabled.store (true, std::memory_order_relaxed);

    for (int i = 0; i < 32; ++i)
        processOneBlock();

    const auto steadyAllocations = allocCount::total.load (std::memory_order_relaxed);

    // A message-thread update, including a tempo change, must reach the next
    // block without allocating on the audio thread.
    ihub::dsp::DelayTransport transport;
    transport.playing = true;
    transport.bpm = 96.0;
    delay.setTransport (transport);
    delay.setDivision (ihub::dsp::DelayDivision::eighth);
    delay.setFeedback (0.6f);

    allocCount::total.store (0, std::memory_order_relaxed);
    processOneBlock();
    const auto updateAllocations = allocCount::total.load (std::memory_order_relaxed);

    allocCount::enabled.store (false, std::memory_order_relaxed);

    std::cout << "      allocations: " << steadyAllocations << " steady, "
              << updateAllocations << " on the update block" << std::endl;

    check (steadyAllocations == 0, "normal processing allocates nothing");
    check (updateAllocations == 0, "a parameter update block allocates nothing");
    check (delay.getDelaySamples() == 7500,
           "the update was applied (1/8 at 96 BPM is 7500 samples)");
}

//==============================================================================
// 8: state round-trip.

void testStateRoundTrip()
{
    std::cout << "\n-- state round-trip --" << std::endl;

    ihub::dsp::Delay source;
    source.prepareToPlay (48000.0, kBlockSize);

    ihub::dsp::DelaySettings settings;
    settings.tempoSync = false;
    settings.division = ihub::dsp::DelayDivision::dottedEighth;
    settings.timeMs = 333.0f;
    settings.feedback = 0.42f;
    settings.mix = 0.66f;
    settings.dampingHz = 4200.0f;
    source.setSettings (settings);
    source.setEnabled (false);

    juce::MemoryBlock state;
    source.getStateInformation (state);

    ihub::dsp::Delay restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));

    const auto restoredSettings = restored.getSettings();

    check (restoredSettings == settings, "settings round-trip");
    check (! restored.isEnabled(), "bypass round-trips");
}

} // namespace

//==============================================================================
int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    testAcceptance();
    testDivisionTable();
    testFreeTime();
    testFeedback();
    testTransportChange();
    testBypassAndDryMix();
    testParameterUpdateIsRealtimeSafe();
    testStateRoundTrip();

    std::cout << (failures == 0 ? "\nPASS" : "\nFAIL") << ": " << failures
              << " failed check(s)" << std::endl;

    return failures == 0 ? 0 : 1;
}
