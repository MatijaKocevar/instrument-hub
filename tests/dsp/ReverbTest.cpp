// ihub-reverb-test: acceptance check for T062 (algorithmic reverb).
//
// T062 has one numeric acceptance sentence, for the delay. The reverb is
// checked against the properties that make an algorithmic reverb correct
// rather than one exact sample count:
//
// 1. A 0% mix and bypass are bit-exact dry passthroughs.
// 2. An impulse produces a tail that decays: the energy in later windows is
//    strictly below earlier ones, and the response reaches -60 dB (relative
//    to its peak) within the reported tail length.
// 3. A larger room size produces a visibly longer tail.
// 4. Damping darkens the tail: the high-frequency energy ratio is lower with
//    heavy damping.
// 5. Pre-delay is silent: with 50 ms of pre-delay, the first 2400 samples of
//    a wet-only impulse response are exactly zero.
// 6. Width controls stereo decorrelation: width 0 makes both channels
//    identical, width 1 makes them differ.
// 7. The network is stable at maximum room size under sustained noise: every
//    output sample is finite and bounded.
// 8. Parameter changes and normal processing allocate nothing on the audio
//    thread.
// 9. State round-trips.

#include "dsp/Reverb.h"

#include <juce_events/juce_events.h>

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
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

//==============================================================================
// Rendering helpers.

void processInBlocks (ihub::dsp::Reverb& reverb,
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

        reverb.processBlock (block, midi);
    }
}

juce::AudioBuffer<float> renderImpulse (ihub::dsp::Reverb& reverb,
                                        double seconds,
                                        double sampleRate = kSampleRate)
{
    const auto numSamples = static_cast<int> (seconds * sampleRate);
    juce::AudioBuffer<float> buffer (2, numSamples);
    buffer.clear();
    buffer.setSample (0, 0, 1.0f);
    buffer.setSample (1, 0, 1.0f);

    processInBlocks (reverb, buffer);
    return buffer;
}

double rms (const juce::AudioBuffer<float>& buffer, int channel, int from, int to)
{
    if (to <= from)
        return 0.0;

    double sum = 0.0;

    for (int i = from; i < to; ++i)
    {
        const auto value = static_cast<double> (buffer.getSample (channel, i));
        sum += value * value;
    }

    return std::sqrt (sum / static_cast<double> (to - from));
}

/** RMS of the sample-to-sample difference divided by the signal RMS. A rough
    high-frequency ratio that does not need an FFT. */
double highFrequencyRatio (const juce::AudioBuffer<float>& buffer, int channel,
                           int from, int to)
{
    double differenceEnergy = 0.0;
    double signalEnergy = 0.0;

    for (int i = juce::jmax (1, from); i < to; ++i)
    {
        const auto current = static_cast<double> (buffer.getSample (channel, i));
        const auto previous = static_cast<double> (buffer.getSample (channel, i - 1));

        differenceEnergy += (current - previous) * (current - previous);
        signalEnergy += current * current;
    }

    if (signalEnergy <= 0.0)
        return 0.0;

    return std::sqrt (differenceEnergy / signalEnergy);
}

/** Last sample above a threshold given as a fraction of the peak. */
int tailLengthSamples (const juce::AudioBuffer<float>& buffer, int channel,
                       float relativeThreshold = 1.0e-4f)
{
    float peak = 0.0f;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
        peak = juce::jmax (peak, std::abs (buffer.getSample (channel, i)));

    const auto threshold = peak * relativeThreshold;
    int last = 0;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
        if (std::abs (buffer.getSample (channel, i)) > threshold)
            last = i;

    return last;
}

ihub::dsp::ReverbSettings makeSettings (float roomSize, float damping,
                                        float width, float mix,
                                        float preDelayMs = 0.0f)
{
    ihub::dsp::ReverbSettings settings;
    settings.roomSize = roomSize;
    settings.damping = damping;
    settings.width = width;
    settings.mix = mix;
    settings.preDelayMs = preDelayMs;
    return settings;
}

//==============================================================================
// 1: dry path.

void testDryPath()
{
    std::cout << "\n-- dry path --" << std::endl;

    ihub::dsp::Reverb reverb;
    reverb.prepareToPlay (kSampleRate, kBlockSize);
    reverb.setSettings (makeSettings (0.6f, 0.5f, 1.0f, 0.0f));

    juce::AudioBuffer<float> signal (2, 4096);

    for (int channel = 0; channel < 2; ++channel)
        for (int i = 0; i < signal.getNumSamples(); ++i)
            signal.setSample (channel, i,
                              std::sin (0.013f * static_cast<float> (i + 3 * channel)));

    juce::AudioBuffer<float> reference;
    reference.makeCopyOf (signal);

    processInBlocks (reverb, signal);

    bool exact = true;

    for (int channel = 0; channel < 2; ++channel)
        for (int i = 0; i < signal.getNumSamples(); ++i)
            if (! juce::exactlyEqual (signal.getSample (channel, i),
                                      reference.getSample (channel, i)))
                exact = false;

    check (exact, "a 0% mix is a bit-exact dry passthrough");

    reverb.setEnabled (false);
    signal.makeCopyOf (reference);
    processInBlocks (reverb, signal);

    exact = true;

    for (int channel = 0; channel < 2; ++channel)
        for (int i = 0; i < signal.getNumSamples(); ++i)
            if (! juce::exactlyEqual (signal.getSample (channel, i),
                                      reference.getSample (channel, i)))
                exact = false;

    check (exact, "bypass is a bit-exact dry passthrough");
    check (reverb.getLatencySamples() == 0, "the reverb reports zero latency");
    check (reverb.getTailLengthSeconds() > 0.0, "the reverb reports a tail length");
}

//==============================================================================
// 2 + 3: decay and room size.

void testDecayAndRoomSize()
{
    std::cout << "\n-- decay and room size --" << std::endl;

    ihub::dsp::Reverb small;
    small.prepareToPlay (kSampleRate, kBlockSize);
    small.setSettings (makeSettings (0.2f, 0.4f, 0.0f, 1.0f));
    const auto smallResponse = renderImpulse (small, 6.0);

    ihub::dsp::Reverb large;
    large.prepareToPlay (kSampleRate, kBlockSize);
    large.setSettings (makeSettings (0.95f, 0.4f, 0.0f, 1.0f));
    const auto largeResponse = renderImpulse (large, 6.0);

    const auto window = static_cast<int> (0.1 * kSampleRate);

    const auto early = rms (largeResponse, 0, 0, 5 * window);
    const auto later = rms (largeResponse, 0, 30 * window, 35 * window);
    const auto latest = rms (largeResponse, 0, 45 * window, 50 * window);

    std::cout << "      rms windows (0-0.5 s, 3.0-3.5 s, 4.5-5.0 s): "
              << early << ", " << later << ", " << latest << std::endl;

    check (early > later && later > latest,
           "the reverb tail decays monotonically across 5 s");

    const auto smallTail = tailLengthSamples (smallResponse, 0);
    const auto largeTail = tailLengthSamples (largeResponse, 0);

    std::cout << "      tail length (-80 dB): small room " << smallTail
              << " samples, large room " << largeTail << " samples" << std::endl;

    check (smallTail > 0, "the small room has a non-empty tail");
    check (largeTail > smallTail,
           "a larger room size produces a longer tail");

    const auto tailSeconds = static_cast<double> (largeTail) / kSampleRate;
    check (tailSeconds < ihub::dsp::Reverb::maxTailSeconds,
           "the tail fits inside the reported tail length");

    // Every sample must be finite.
    bool finite = true;

    for (int channel = 0; channel < 2; ++channel)
        for (int i = 0; i < largeResponse.getNumSamples(); ++i)
            if (! std::isfinite (largeResponse.getSample (channel, i)))
                finite = false;

    check (finite, "the impulse response contains only finite samples");
}

//==============================================================================
// 4: damping.

void testDamping()
{
    std::cout << "\n-- damping --" << std::endl;

    ihub::dsp::Reverb bright;
    bright.prepareToPlay (kSampleRate, kBlockSize);
    bright.setSettings (makeSettings (0.7f, 0.0f, 0.0f, 1.0f));
    const auto brightResponse = renderImpulse (bright, 3.0);

    ihub::dsp::Reverb dark;
    dark.prepareToPlay (kSampleRate, kBlockSize);
    dark.setSettings (makeSettings (0.7f, 0.95f, 0.0f, 1.0f));
    const auto darkResponse = renderImpulse (dark, 3.0);

    const auto from = static_cast<int> (0.2 * kSampleRate);
    const auto to = static_cast<int> (0.8 * kSampleRate);

    const auto brightRatio = highFrequencyRatio (brightResponse, 0, from, to);
    const auto darkRatio = highFrequencyRatio (darkResponse, 0, from, to);

    std::cout << "      high-frequency ratio: bright " << brightRatio
              << ", damped " << darkRatio << std::endl;

    check (darkRatio < brightRatio,
           "heavy damping lowers the high-frequency content of the tail");
}

//==============================================================================
// 5: pre-delay.

void testPreDelay()
{
    std::cout << "\n-- pre-delay --" << std::endl;

    ihub::dsp::Reverb reverb;
    reverb.prepareToPlay (kSampleRate, kBlockSize);
    reverb.setSettings (makeSettings (0.6f, 0.4f, 0.0f, 1.0f, 50.0f));

    const auto response = renderImpulse (reverb, 2.0);
    const auto preDelaySamples = static_cast<int> (0.05 * kSampleRate);

    float leaked = 0.0f;

    for (int i = 0; i < preDelaySamples; ++i)
        leaked = juce::jmax (leaked, std::abs (response.getSample (0, i)),
                             std::abs (response.getSample (1, i)));

    const auto after = rms (response, 0, preDelaySamples,
                            preDelaySamples + 4000);

    std::cout << "      50 ms pre-delay: leaked " << leaked
              << " before sample " << preDelaySamples
              << ", tail rms after: " << after << std::endl;

    check (leaked <= 0.0f,
           "the wet output is exactly silent before the pre-delay");
    check (after > 1.0e-6,
           "the tail starts after the pre-delay");
}

//==============================================================================
// 6: width.

void testWidth()
{
    std::cout << "\n-- stereo width --" << std::endl;

    ihub::dsp::Reverb mono;
    mono.prepareToPlay (kSampleRate, kBlockSize);
    mono.setSettings (makeSettings (0.6f, 0.4f, 0.0f, 1.0f));
    const auto monoResponse = renderImpulse (mono, 2.0);

    ihub::dsp::Reverb wide;
    wide.prepareToPlay (kSampleRate, kBlockSize);
    wide.setSettings (makeSettings (0.6f, 0.4f, 1.0f, 1.0f));
    const auto wideResponse = renderImpulse (wide, 2.0);

    float maxDifferenceNarrow = 0.0f;
    float maxDifferenceWide = 0.0f;

    for (int i = 0; i < monoResponse.getNumSamples(); ++i)
    {
        maxDifferenceNarrow = juce::jmax (maxDifferenceNarrow,
                                          std::abs (monoResponse.getSample (0, i)
                                                    - monoResponse.getSample (1, i)));
        maxDifferenceWide = juce::jmax (maxDifferenceWide,
                                        std::abs (wideResponse.getSample (0, i)
                                                  - wideResponse.getSample (1, i)));
    }

    std::cout << "      max L/R difference: width 0 " << maxDifferenceNarrow
              << ", width 1 " << maxDifferenceWide << std::endl;

    check (maxDifferenceNarrow < 1.0e-6f,
           "width 0 produces identical left and right channels");
    check (maxDifferenceWide > 1.0e-4f,
           "width 1 decorrelates the left and right channels");
}

//==============================================================================
// 7: stability.

void testStability()
{
    std::cout << "\n-- stability at maximum room size --" << std::endl;

    ihub::dsp::Reverb reverb;
    reverb.prepareToPlay (kSampleRate, kBlockSize);
    reverb.setSettings (makeSettings (1.0f, 0.0f, 1.0f, 1.0f));

    const auto seconds = 5;
    const auto numSamples = seconds * static_cast<int> (kSampleRate);

    juce::AudioBuffer<float> noise (2, numSamples);

    // Deterministic LCG so the test never depends on the standard library's
    // distribution details.
    std::uint32_t state = 0x12345678u;

    const auto next = [&state] () noexcept
    {
        state = state * 1664525u + 1013904223u;
        return (static_cast<float> (state) / 2147483648.0f) - 1.0f;
    };

    for (int channel = 0; channel < 2; ++channel)
        for (int i = 0; i < numSamples; ++i)
            noise.setSample (channel, i, 0.5f * next());

    processInBlocks (reverb, noise);

    float peak = 0.0f;
    bool finite = true;

    for (int channel = 0; channel < 2; ++channel)
        for (int i = 0; i < numSamples; ++i)
        {
            const auto value = noise.getSample (channel, i);
            finite = finite && std::isfinite (value);
            peak = juce::jmax (peak, std::abs (value));
        }

    std::cout << "      peak after 5 s of noise at room size 1.0: " << peak
              << std::endl;

    check (finite, "all output samples are finite");
    check (peak < 8.0f, "the network does not run away at maximum room size");

    // Silence in, silence out: the tail must eventually die. A room size of
    // 0.6 decays fast enough to be gone within the reported tail length; at
    // 1.0 the Freeverb feedback is deliberately very long.
    ihub::dsp::Reverb closing;
    closing.prepareToPlay (kSampleRate, kBlockSize);
    closing.setSettings (makeSettings (0.6f, 0.4f, 1.0f, 1.0f));

    juce::AudioBuffer<float> tail (2, static_cast<int> (1.0 * kSampleRate));
    tail.clear();
    tail.setSample (0, 0, 1.0f);
    tail.setSample (1, 0, 1.0f);
    processInBlocks (closing, tail);

    juce::AudioBuffer<float> silence (2, static_cast<int> (8.0 * kSampleRate));
    silence.clear();
    processInBlocks (closing, silence);

    const auto last = rms (silence, 0, silence.getNumSamples() - 4096,
                           silence.getNumSamples());

    std::cout << "      rms after 8 s of silence: " << last << std::endl;
    check (last < 1.0e-5, "the tail decays to silence");
}

//==============================================================================
// 8: realtime safety.

void testParameterUpdateIsRealtimeSafe()
{
    std::cout << "\n-- realtime safety --" << std::endl;

    ihub::dsp::Reverb reverb;
    reverb.prepareToPlay (kSampleRate, kBlockSize);

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    juce::MidiBuffer midi;

    buffer.clear();
    buffer.setSample (0, 0, 1.0f);

    const auto processOneBlock = [&]
    {
        reverb.processBlock (buffer, midi);
    };

    for (int i = 0; i < 4; ++i)
        processOneBlock();

    allocCount::total.store (0, std::memory_order_relaxed);
    allocCount::enabled.store (true, std::memory_order_relaxed);

    for (int i = 0; i < 32; ++i)
        processOneBlock();

    const auto steadyAllocations = allocCount::total.load (std::memory_order_relaxed);

    reverb.setRoomSize (0.85f);
    reverb.setDamping (0.2f);
    reverb.setPreDelayMs (30.0f);

    allocCount::total.store (0, std::memory_order_relaxed);
    processOneBlock();
    const auto updateAllocations = allocCount::total.load (std::memory_order_relaxed);

    allocCount::enabled.store (false, std::memory_order_relaxed);

    std::cout << "      allocations: " << steadyAllocations << " steady, "
              << updateAllocations << " on the update block" << std::endl;

    check (steadyAllocations == 0, "normal processing allocates nothing");
    check (updateAllocations == 0, "a parameter update block allocates nothing");
    check (std::abs (reverb.getSettings().preDelayMs - 30.0f) < 1.0e-6f,
           "the parameter update was stored");
}

//==============================================================================
// 9: state round-trip.

void testStateRoundTrip()
{
    std::cout << "\n-- state round-trip --" << std::endl;

    ihub::dsp::Reverb source;
    source.prepareToPlay (kSampleRate, kBlockSize);

    const auto settings = makeSettings (0.77f, 0.22f, 0.33f, 0.44f, 55.0f);
    source.setSettings (settings);
    source.setEnabled (false);

    juce::MemoryBlock state;
    source.getStateInformation (state);

    ihub::dsp::Reverb restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));

    check (restored.getSettings() == settings, "settings round-trip");
    check (! restored.isEnabled(), "bypass round-trips");
}

} // namespace

//==============================================================================
int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    testDryPath();
    testDecayAndRoomSize();
    testDamping();
    testPreDelay();
    testWidth();
    testStability();
    testParameterUpdateIsRealtimeSafe();
    testStateRoundTrip();

    std::cout << (failures == 0 ? "\nPASS" : "\nFAIL") << ": " << failures
              << " failed check(s)" << std::endl;

    return failures == 0 ? 0 : 1;
}
