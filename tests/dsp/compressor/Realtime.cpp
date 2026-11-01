#include "Realtime.h"

#include "Fixtures.h"

#include "../AllocationCount.h"

namespace DspTest::Compressor
{

void runRealtime()
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

    juce::AudioBuffer<float> warmup (1, static_cast<int> (kSampleRate));
    fillTone (warmup, 0, -12.0, kToneFrequency, kSampleRate);
    processInBlocks (compressor, warmup, kBlockSize);

    check (std::abs (compressor.getGainReductionDb() - 4.5f) <= 1.0f,
           "warm-up reaches the old static curve");

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

    juce::AudioBuffer<float> settle (1, static_cast<int> (0.5 * kSampleRate));
    fillTone (settle, 0, -12.0, kToneFrequency, kSampleRate);
    processInBlocks (compressor, settle, kBlockSize);

    const auto settledGr = compressor.getGainReductionDb();
    std::cout << "      new threshold -> " << settledGr << " dB (expected 13.5 dB)"
              << std::endl;

    check (std::abs (settledGr - 13.5f) <= 1.0f,
           "the update is audible on the next block and reaches its static curve");
}

}
