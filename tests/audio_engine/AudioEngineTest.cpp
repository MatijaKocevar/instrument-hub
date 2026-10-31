// ihub-audio-engine-test: acceptance check for T011 (fixed slot graph).
//
// 1. Renders exactly 2 s through an AudioEngine with three active slots and
//    compares every sample against a manually mixed reference. The comparison
//    starts after the 3 ms activation fade, and requires the difference to be
//    below -90 dBFS.
// 2. Proves that only slots in a fading/active state are processed.
// 3. Counts heap allocations across processBlock() by replacing the global
//    operator new and wrapping malloc/calloc/realloc, and requires zero.
// 4. Checks the master limiter holds peaks at its threshold.

#include "audio/AudioEngine.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
#include <string>
#include <vector>

//==============================================================================
// Counting allocator
//
// Every allocation in the binary goes through this TU's operator new, and on
// Linux the link step additionally redirects malloc/calloc/realloc to the
// __wrap_* below. Both bump the same counter while a measurement window is
// open; outside the window the hook is a single relaxed atomic load.

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
int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

double dbfs (double linear)
{
    if (linear <= 0.0)
        return -200.0;

    return 20.0 * std::log10 (linear);
}

/** A deterministic sine source with a known closed form. */
class SineSource final : public AudioEngine::SlotSource
{
public:
    SineSource (double frequencyToUse, float amplitudeToUse, double phaseToUse = 0.0)
        : frequency (frequencyToUse),
          amplitude (amplitudeToUse),
          phase (phaseToUse)
    {
    }

    void prepare (double newSampleRate, int) override
    {
        sampleRate = newSampleRate;
        position = 0;
    }

    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        processCalls.fetch_add (1, std::memory_order_relaxed);

        const int samples = buffer.getNumSamples();
        const int channels = buffer.getNumChannels();

        for (int i = 0; i < samples; ++i)
        {
            const float sample = static_cast<float> (valueAt (position + i));

            for (int channel = 0; channel < channels; ++channel)
                buffer.getWritePointer (channel)[i] = sample;
        }

        position += samples;
    }

    /** The same sample the renderer would have produced at an absolute index. */
    double referenceAt (std::int64_t index) const
    {
        return valueAt (index);
    }

    int latencySamples() const noexcept override { return latency; }

    std::atomic<int> processCalls { 0 };
    int latency = 0;

private:
    double valueAt (std::int64_t index) const
    {
        if (sampleRate <= 0.0)
            return 0.0;

        const double phaseNow = phase
                                + juce::MathConstants<double>::twoPi * frequency
                                      * static_cast<double> (index) / sampleRate;

        return static_cast<double> (amplitude) * std::sin (phaseNow);
    }

    double sampleRate = 48000.0;
    double frequency = 440.0;
    float amplitude = 0.1f;
    double phase = 0.0;
    std::int64_t position = 0;
};

/** Mixes the sources by hand, exactly as the bus is supposed to. */
double referenceSample (const std::vector<std::unique_ptr<SineSource>>& sources,
                        const std::vector<float>& gains,
                        std::int64_t index)
{
    double sum = 0.0;

    for (size_t i = 0; i < sources.size(); ++i)
        sum += static_cast<double> (gains[i]) * sources[i]->referenceAt (index);

    return sum;
}

//==============================================================================
void testMixAgainstReference()
{
    std::cout << "\n-- mix against manual reference --" << std::endl;

    constexpr double sampleRate = 48000.0;
    constexpr int maxBlock = 512;
    constexpr std::int64_t totalSamples = 96000; // exactly 2 s
    constexpr std::int64_t compareFrom = 4096;   // well past the 3 ms fade

    AudioEngine::AudioEngine engine (3, 2);

    std::vector<std::unique_ptr<SineSource>> sources;
    sources.push_back (std::make_unique<SineSource> (220.0, 0.20f, 0.0));
    sources.push_back (std::make_unique<SineSource> (330.0, 0.15f, 1.1));
    sources.push_back (std::make_unique<SineSource> (440.0, 0.10f, 2.3));

    const std::vector<float> gains { 1.0f, 0.8f, 0.5f };

    for (int index = 0; index < 3; ++index)
    {
        engine.getSlot (index).setSource (sources[static_cast<size_t> (index)].get());
        engine.getSlot (index).setGain (gains[static_cast<size_t> (index)]);
        engine.getSlot (index).activate();
    }

    engine.prepare (sampleRate, maxBlock, 2);

    juce::AudioBuffer<float> output (2, maxBlock);
    juce::MidiBuffer midi;

    double sumSquaredDifference = 0.0;
    double maxDifference = 0.0;
    double referencePeak = 0.0;
    std::int64_t compared = 0;
    std::int64_t rendered = 0;

    while (rendered < totalSamples)
    {
        const auto remaining = totalSamples - rendered;
        const int samples = static_cast<int> (std::min<std::int64_t> (maxBlock, remaining));

        engine.processBlock (output, midi, samples);

        for (int i = 0; i < samples; ++i)
        {
            const std::int64_t absolute = rendered + i;

            if (absolute < compareFrom)
                continue;

            const double reference = referenceSample (sources, gains, absolute);
            referencePeak = std::max (referencePeak, std::abs (reference));

            for (int channel = 0; channel < 2; ++channel)
            {
                const double difference = static_cast<double> (output.getSample (channel, i))
                                          - reference;

                sumSquaredDifference += difference * difference;
                maxDifference = std::max (maxDifference, std::abs (difference));
                ++compared;
            }
        }

        rendered += samples;
    }

    const double rmsDifference = compared > 0
                                     ? std::sqrt (sumSquaredDifference / static_cast<double> (compared))
                                     : 0.0;
    const double rmsDb = dbfs (rmsDifference);

    std::cout << "rendered " << rendered << " samples, compared " << compared
              << " samples from " << compareFrom
              << ", reference peak " << referencePeak
              << ", max difference " << maxDifference
              << ", RMS difference " << rmsDb << " dBFS" << std::endl;

    check (rendered == totalSamples, "rendered exactly 2 s at 48 kHz (96000 samples)");
    check (referencePeak > 0.3, "reference mix has signal (peak > 0.3)");
    check (rmsDb < -90.0, "mix matches the manual reference within -90 dBFS");
    check (maxDifference < 1.0e-4, "no individual sample differs by more than 1e-4");
    check (sources[0]->processCalls.load() > 0
               && sources[1]->processCalls.load() > 0
               && sources[2]->processCalls.load() > 0,
           "all three slots were processed");
}

//==============================================================================
void testOnlyActiveAndFadingSlotsProcess()
{
    std::cout << "\n-- only active or fading slots are processed --" << std::endl;

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 64;

    AudioEngine::AudioEngine engine (3, 2);

    auto first = std::make_unique<SineSource> (110.0, 0.1f);
    auto second = std::make_unique<SineSource> (220.0, 0.1f);
    auto third = std::make_unique<SineSource> (330.0, 0.1f);

    second->latency = 64;

    engine.getSlot (0).setSource (first.get());
    engine.getSlot (1).setSource (second.get());
    engine.getSlot (2).setSource (third.get());

    engine.prepare (sampleRate, blockSize, 2);

    check (engine.getLatencySamples() == 64,
           "engine reports the largest source latency");

    juce::AudioBuffer<float> output (2, blockSize);
    juce::MidiBuffer midi;

    // Everyone idle: nothing may run.
    for (int i = 0; i < 8; ++i)
        engine.processBlock (output, midi);

    check (first->processCalls.load() == 0
               && second->processCalls.load() == 0
               && third->processCalls.load() == 0,
           "idle slots are never processed");
    check (output.getMagnitude (0, blockSize) <= 0.0f, "idle graph renders silence");

    // Activate slot 0. The 3 ms ramp is 144 samples at 48 kHz, so it takes
    // 3 blocks of 64 samples to reach `active`.
    engine.getSlot (0).activate();

    engine.processBlock (output, midi);
    check (engine.getSlot (0).getState() == AudioEngine::SlotState::fadingIn,
           "slot reports fading-in during the ramp");
    check (engine.getSlot (0).isProcessing(), "a fading slot processes audio");

    engine.processBlock (output, midi);
    engine.processBlock (output, midi);

    check (engine.getSlot (0).getState() == AudioEngine::SlotState::active,
           "slot reaches active after the fade");

    // Deactivate: it must keep processing for the fade, then stop.
    engine.getSlot (0).deactivate();

    const int callsBeforeFadeOut = first->processCalls.load();
    engine.processBlock (output, midi);

    check (engine.getSlot (0).getState() == AudioEngine::SlotState::fadingOut,
           "slot reports fading-out during the ramp");
    check (first->processCalls.load() > callsBeforeFadeOut,
           "a fading-out slot still processes audio");

    engine.processBlock (output, midi);
    engine.processBlock (output, midi);

    check (engine.getSlot (0).getState() == AudioEngine::SlotState::idle,
           "slot returns to idle after the fade");

    const int callsAfterFadeOut = first->processCalls.load();

    for (int i = 0; i < 8; ++i)
        engine.processBlock (output, midi);

    check (first->processCalls.load() == callsAfterFadeOut,
           "idle slot stops receiving processBlock()");
    check (output.getMagnitude (0, blockSize) <= 0.0f,
           "output is silent once the slot faded out");
    check (second->processCalls.load() == 0 && third->processCalls.load() == 0,
           "slots that never activated stay unprocessed");
}

//==============================================================================
void testNoAllocationsDuringProcessBlock()
{
    std::cout << "\n-- zero allocations in processBlock --" << std::endl;

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 256;
    constexpr int measuredBlocks = 64;

    AudioEngine::AudioEngine engine (3, 2);

    std::vector<std::unique_ptr<SineSource>> sources;
    sources.push_back (std::make_unique<SineSource> (220.0, 0.2f, 0.0));
    sources.push_back (std::make_unique<SineSource> (330.0, 0.15f, 0.7));
    sources.push_back (std::make_unique<SineSource> (440.0, 0.1f, 1.4));

    for (int index = 0; index < 3; ++index)
    {
        engine.getSlot (index).setSource (sources[static_cast<size_t> (index)].get());
        engine.getSlot (index).activate();
    }

    engine.prepare (sampleRate, blockSize, 2);

    juce::AudioBuffer<float> output (2, blockSize);
    juce::MidiBuffer midi;

    // Warm every lazily-initialised path (and finish the fade) before the
    // measurement window opens.
    for (int i = 0; i < 8; ++i)
        engine.processBlock (output, midi);

    const int callsBefore = sources[0]->processCalls.load();

    allocCount::total.store (0, std::memory_order_relaxed);
    allocCount::enabled.store (true, std::memory_order_seq_cst);

    for (int i = 0; i < measuredBlocks; ++i)
        engine.processBlock (output, midi);

    allocCount::enabled.store (false, std::memory_order_seq_cst);

    const long allocations = allocCount::total.load (std::memory_order_relaxed);
    const int callsAfter = sources[0]->processCalls.load();

    std::cout << "allocations during " << measuredBlocks << " blocks: "
              << allocations << " (processBlock calls on slot 0: "
              << (callsAfter - callsBefore) << ")" << std::endl;

    check (callsAfter - callsBefore == measuredBlocks,
           "the measured window really ran processBlock on the active slots");
    check (allocations == 0, "zero heap allocations during processBlock");

    // Prove the counter is live, so a passing result above is meaningful.
    allocCount::total.store (0, std::memory_order_relaxed);
    allocCount::enabled.store (true, std::memory_order_seq_cst);

    {
        auto* probe = new float[16];
        delete[] probe;
    }

    allocCount::enabled.store (false, std::memory_order_seq_cst);

    check (allocCount::total.load (std::memory_order_relaxed) > 0,
           "counting allocator detects a deliberate allocation");
}

//==============================================================================
void testMasterLimiter()
{
    std::cout << "\n-- master limiter --" << std::endl;

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 256;

    AudioEngine::AudioEngine engine (2, 2);

    auto low = std::make_unique<SineSource> (110.0, 0.8f);
    auto high = std::make_unique<SineSource> (157.0, 0.8f);

    engine.getSlot (0).setSource (low.get());
    engine.getSlot (1).setSource (high.get());
    engine.getSlot (0).activate();
    engine.getSlot (1).activate();

    engine.prepare (sampleRate, blockSize, 2);

    juce::AudioBuffer<float> output (2, blockSize);
    juce::MidiBuffer midi;

    for (int i = 0; i < 8; ++i)
        engine.processBlock (output, midi);

    float outputPeak = 0.0f;
    float lowestGainReductionDb = 0.0f;

    for (int i = 0; i < 32; ++i)
    {
        engine.processBlock (output, midi);
        outputPeak = juce::jmax (outputPeak, output.getMagnitude (0, blockSize));
        lowestGainReductionDb = juce::jmin (
            lowestGainReductionDb,
            engine.getBus().getLimiter().getGainReductionDb());
    }

    const float thresholdGain = juce::Decibels::decibelsToGain (
        engine.getBus().getLimiter().getThresholdDb(), -100.0f);

    std::cout << "output peak " << outputPeak << ", threshold " << thresholdGain
              << ", gain reduction " << lowestGainReductionDb << " dB" << std::endl;

    check (outputPeak > 0.9f, "limiter test produces a hot signal");
    check (outputPeak <= thresholdGain + 1.0e-4f,
           "limiter keeps the output at or below the threshold");
    check (lowestGainReductionDb < -1.0f,
           "limiter reports meaningful gain reduction");
}
} // namespace

//==============================================================================
int main()
{
    std::cout << "ihub-audio-engine-test: T011 fixed slot graph" << std::endl;

    testMixAgainstReference();
    testOnlyActiveAndFadingSlotsProcess();
    testNoAllocationsDuringProcessBlock();
    testMasterLimiter();

    std::cout << (failures == 0 ? "\nPASS" : "\nFAIL")
              << ": ihub-audio-engine (" << failures << " failure(s))"
              << std::endl;

    return failures == 0 ? 0 : 1;
}
