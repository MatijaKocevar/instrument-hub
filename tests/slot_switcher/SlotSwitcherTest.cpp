// ihub-slot-switcher-test: acceptance check for T032.
//
// 1. Two sine-generating slots are attached to a SlotSwitcher. Slot 0 is armed
//    and settles to `active`; arming slot 1 publishes `fading` + `warming`
//    while the 3 ms ramp runs, then `idle` + `active`.
// 2. The rendered output is compared against a continuous equal-power
//    reference. Both sources produce the same sine (phase aligned at the
//    switch), so the reference is simply sine * [cos(p) + sin(p)] during the
//    fade. The worst absolute deviation is reported in dBFS and must be below
//    -60 dBFS. A hard switch or a broken ramp leaves a sample-sized residual.
// 3. The published mid-fade gains satisfy gOut^2 + gIn^2 = 1 (equal power).
// 4. process() performs no heap allocation.

#include "audio/Slot.h"
#include "audio/SlotSwitcher.h"

#include <juce_events/juce_events.h>

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <new>
#include <string>
#include <vector>

//==============================================================================
// Counting allocator (same pattern as ihub-audio-engine-test).

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
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 128;
constexpr int kNumChannels = 2;
constexpr float kFadeMilliseconds = 3.0f;
constexpr int kFadeSamples = 144; // 3 ms at 48 kHz

constexpr double kFrequency = 220.0;
constexpr float kAmplitude = 0.5f;

constexpr int kPreRollBlocks = 2;
constexpr int kSwitchBlock = 12;
constexpr int kRecordedBlocks = 24;

constexpr int kPreRollSamples = kPreRollBlocks * kBlockSize;
constexpr int kSwitchSample = kSwitchBlock * kBlockSize;
constexpr int kRecordedSamples = kRecordedBlocks * kBlockSize;

int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

/** A phase-continuous sine on every channel. */
class SineSource final : public AudioEngine::SlotSource
{
public:
    SineSource (float amplitudeToUse, double frequencyToUse, double phaseOffset)
        : amplitude (amplitudeToUse),
          frequency (frequencyToUse),
          initialPhase (phaseOffset)
    {
    }

    void prepare (double newSampleRate, int) override
    {
        sampleRate = newSampleRate;
        phase = initialPhase;
        increment = juce::MathConstants<double>::twoPi * frequency / sampleRate;
    }

    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        for (int frame = 0; frame < buffer.getNumSamples(); ++frame)
        {
            const auto value = static_cast<float> (amplitude * std::sin (phase));
            phase += increment;

            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                buffer.getWritePointer (channel)[frame] = value;
        }
    }

    double getPhase() const noexcept { return phase; }

private:
    float amplitude;
    double frequency;
    double initialPhase;

    double sampleRate = 48000.0;
    double phase = 0.0;
    double increment = 0.0;
};

struct ContinuityResult
{
    double maxResidual = 0.0;
    double maxResidualDbfs = 0.0;
    double maxOutputJump = 0.0;
};

ContinuityResult runSwitchTest (std::vector<float>& output, bool& producedEveryBlock)
{
    const double increment = juce::MathConstants<double>::twoPi * kFrequency / kSampleRate;
    const double preRollPhase = std::fmod (kPreRollSamples * increment,
                                           juce::MathConstants<double>::twoPi);
    const double switchPhase = std::fmod (preRollPhase + kSwitchSample * increment,
                                          juce::MathConstants<double>::twoPi);

    // A starts at phase 0 and runs through the pre-roll. B stays idle until the
    // switch, so its start phase is chosen to line up with A at the switch.
    SineSource sourceA (kAmplitude, kFrequency, 0.0);
    SineSource sourceB (kAmplitude, kFrequency, switchPhase);

    AudioEngine::Slot slotA (0);
    AudioEngine::Slot slotB (1);
    slotA.setSource (&sourceA);
    slotB.setSource (&sourceB);

    AudioEngine::SlotSwitcher switcher (kFadeMilliseconds);
    switcher.setSlots ({ &slotA, &slotB });
    switcher.prepare (kSampleRate, kBlockSize, kNumChannels);

    check (switcher.getNumSlots() == 2, "two slots are attached");
    check (switcher.getSlotState (0) == AudioEngine::SlotSwitcher::State::idle,
           "slot 0 starts idle");
    check (switcher.getSlotState (1) == AudioEngine::SlotSwitcher::State::idle,
           "slot 1 starts idle");

    switcher.armSlot (0);
    check (switcher.getArmedSlot() == 0, "armSlot(0) publishes the request");

    juce::MidiBuffer midi;
    producedEveryBlock = true;

    for (int block = 0; block < kPreRollBlocks; ++block)
        if (! switcher.process (kBlockSize, midi))
            producedEveryBlock = false;

    check (switcher.getSlotState (0) == AudioEngine::SlotSwitcher::State::active,
           "slot 0 is active after its fade-in");
    check (switcher.getSlotState (1) == AudioEngine::SlotSwitcher::State::idle,
           "slot 1 remains idle before it is armed");
    check (std::abs (switcher.getSlotGain (0) - 1.0f) < 1.0e-6f,
           "active slot gain is 1.0");

    output.clear();
    output.reserve (static_cast<size_t> (kRecordedSamples));

    bool sawMidFade = false;

    for (int block = 0; block < kRecordedBlocks; ++block)
    {
        if (block == kSwitchBlock)
        {
            switcher.armSlot (1);
            check (switcher.getArmedSlot() == 1, "armSlot(1) publishes the switch");
        }

        if (! switcher.process (kBlockSize, midi))
            producedEveryBlock = false;

        if (block == kSwitchBlock)
        {
            check (switcher.getSlotState (0) == AudioEngine::SlotSwitcher::State::fading,
                   "the outgoing slot is fading during the crossfade");
            check (switcher.getSlotState (1) == AudioEngine::SlotSwitcher::State::warming,
                   "the incoming slot is warming during the crossfade");

            const float outGain = switcher.getSlotGain (0);
            const float inGain = switcher.getSlotGain (1);
            const float power = outGain * outGain + inGain * inGain;

            check (outGain > 0.0f && inGain > 0.0f,
                   "both slots carry signal at the first fade block");
            check (std::abs (power - 1.0f) < 1.0e-4f,
                   "mid-fade gains are equal-power (gOut^2 + gIn^2 == 1)");
            sawMidFade = true;
        }

        if (block == kSwitchBlock + 1)
        {
            check (switcher.getSlotState (0) == AudioEngine::SlotSwitcher::State::idle,
                   "the outgoing slot is idle once the ramp completes");
            check (switcher.getSlotState (1) == AudioEngine::SlotSwitcher::State::active,
                   "the incoming slot is active once the ramp completes");
        }

        const auto& mixed = switcher.getOutputBuffer();

        for (int frame = 0; frame < kBlockSize; ++frame)
            output.push_back (mixed.getSample (0, frame));
    }

    check (sawMidFade, "the crossfade was observed");

    ContinuityResult result;
    double previousOutput = 0.0;

    for (int n = 0; n < static_cast<int> (output.size()); ++n)
    {
        const double phase = preRollPhase + static_cast<double> (n) * increment;
        const double sine = static_cast<double> (kAmplitude) * std::sin (phase);

        double gainA = 1.0;
        double gainB = 0.0;

        if (n >= kSwitchSample && n < kSwitchSample + kFadeSamples)
        {
            const double p = static_cast<double> (n - kSwitchSample)
                             / static_cast<double> (kFadeSamples);
            gainA = std::cos (p * juce::MathConstants<double>::halfPi);
            gainB = std::sin (p * juce::MathConstants<double>::halfPi);
        }
        else if (n >= kSwitchSample + kFadeSamples)
        {
            gainA = 0.0;
            gainB = 1.0;
        }

        const double expected = sine * (gainA + gainB);
        const double actual = output[static_cast<size_t> (n)];

        result.maxResidual = juce::jmax (result.maxResidual, std::abs (actual - expected));

        if (n > 0)
            result.maxOutputJump = juce::jmax (result.maxOutputJump,
                                               std::abs (actual - previousOutput));

        previousOutput = actual;
    }

    result.maxResidualDbfs = result.maxResidual > 0.0
        ? 20.0 * std::log10 (result.maxResidual)
        : -1000.0;

    return result;
}

void testContinuity()
{
    std::cout << "\n-- equal-power switching between two sine slots --" << std::endl;

    std::vector<float> output;
    bool producedEveryBlock = false;
    const auto result = runSwitchTest (output, producedEveryBlock);

    check (producedEveryBlock, "the switcher renders during and after the switch");

    std::cout << "      worst deviation from continuous reference: "
              << result.maxResidualDbfs << " dBFS" << std::endl;
    std::cout << "      largest output sample-to-sample jump: "
              << result.maxOutputJump << std::endl;

    check (result.maxResidual < 1.0e-3,
           "no discontinuity above -60 dBFS (residual < 1e-3)");
    check (result.maxResidualDbfs < -60.0,
           "worst deviation is below -60 dBFS");
}

void testNoAllocations()
{
    std::cout << "\n-- audio-thread allocation check --" << std::endl;

    SineSource sourceA (kAmplitude, kFrequency, 0.0);
    SineSource sourceB (kAmplitude, kFrequency, 1.0);

    AudioEngine::Slot slotA (0);
    AudioEngine::Slot slotB (1);
    slotA.setSource (&sourceA);
    slotB.setSource (&sourceB);

    AudioEngine::SlotSwitcher switcher (kFadeMilliseconds);
    switcher.setSlots ({ &slotA, &slotB });
    switcher.prepare (kSampleRate, kBlockSize, kNumChannels);

    juce::MidiBuffer midi;

    switcher.armSlot (0);

    for (int block = 0; block < kPreRollBlocks; ++block)
        switcher.process (kBlockSize, midi);

    allocCount::enabled.store (true, std::memory_order_relaxed);
    const long before = allocCount::total.load (std::memory_order_relaxed);

    for (int block = 0; block < 64; ++block)
    {
        if (block == 16)
            switcher.armSlot (1);

        if (block == 48)
            switcher.armSlot (0);

        switcher.process (kBlockSize, midi);
    }

    allocCount::enabled.store (false, std::memory_order_relaxed);
    const long after = allocCount::total.load (std::memory_order_relaxed);

    check (after == before, "process() allocates nothing across three switches");
}

void testDisarm()
{
    std::cout << "\n-- disarm fades the armed slot out --" << std::endl;

    SineSource sourceA (kAmplitude, kFrequency, 0.0);
    AudioEngine::Slot slotA (0);
    slotA.setSource (&sourceA);

    AudioEngine::SlotSwitcher switcher (kFadeMilliseconds);
    switcher.setSlots ({ &slotA });
    switcher.prepare (kSampleRate, kBlockSize, kNumChannels);

    juce::MidiBuffer midi;
    switcher.armSlot (0);

    for (int block = 0; block < kPreRollBlocks; ++block)
        switcher.process (kBlockSize, midi);

    check (switcher.getSlotState (0) == AudioEngine::SlotSwitcher::State::active,
           "slot 0 is active before disarm");

    switcher.disarm();
    check (switcher.getArmedSlot() == -1, "disarm clears the request");

    switcher.process (kBlockSize, midi);
    check (switcher.getSlotState (0) == AudioEngine::SlotSwitcher::State::fading,
           "the slot fades out after disarm");

    for (int block = 0; block < 2; ++block)
        switcher.process (kBlockSize, midi);

    check (switcher.getSlotState (0) == AudioEngine::SlotSwitcher::State::idle,
           "the slot is idle after the fade completes");
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    testContinuity();
    testNoAllocations();
    testDisarm();

    std::cout << (failures == 0 ? "\nPASS" : "\nFAIL") << ": " << failures
              << " failed check(s)" << std::endl;

    return failures == 0 ? 0 : 1;
}
