#include "Realtime.h"

#include "Fixtures.h"

#include "../AllocationCount.h"

namespace DspTest::Reverb
{

void runRealtime()
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

}
