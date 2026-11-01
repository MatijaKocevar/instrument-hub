#include "Realtime.h"
#include "AllocationCount.h"
#include "Fixtures.h"

#include <iostream>
#include <memory>
#include <vector>

namespace AudioEngineTest
{

namespace
{

void* volatile allocationSink = nullptr;

}

void runNoAllocationsDuringProcessBlock()
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

    allocCount::total.store (0, std::memory_order_relaxed);
    allocCount::enabled.store (true, std::memory_order_seq_cst);

    {
        auto* probe = new float[16];
        allocationSink = probe;
        delete[] probe;
        allocationSink = nullptr;
    }

    allocCount::enabled.store (false, std::memory_order_seq_cst);

    check (allocCount::total.load (std::memory_order_relaxed) > 0,
           "counting allocator detects a deliberate allocation");
}

}
