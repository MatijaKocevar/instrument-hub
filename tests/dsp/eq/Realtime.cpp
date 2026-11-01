#include "Realtime.h"

#include "Fixtures.h"

#include "../AllocationCount.h"

namespace DspTest::Eq
{

void runRealtime()
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

} // namespace DspTest::Eq
