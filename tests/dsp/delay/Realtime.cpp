#include "Realtime.h"

#include "Fixtures.h"

#include "../AllocationCount.h"

namespace DspTest::Delay
{

void runRealtime()
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

} // namespace DspTest::Delay
