#include "Render.h"

#include "SineSource.h"

#include "audio/Bus.h"
#include "audio/Slot.h"
#include "audio/SlotSwitcher.h"

#include <juce_events/juce_events.h>

#include <cmath>
#include <memory>
#include <random>
#include <vector>

namespace AudioTest::SwitchContinuity
{

RunResult renderRun (float fadeMilliseconds)
{
    RunResult result;

    std::vector<std::unique_ptr<SineSource>> sources;
    std::vector<std::unique_ptr<AudioEngine::Slot>> slots;
    std::vector<AudioEngine::Slot*> slotPointers;

    for (int index = 0; index < kNumSlots; ++index)
    {
        sources.push_back (std::make_unique<SineSource> (kFrequencies[static_cast<size_t> (index)],
                                                        kAmplitude,
                                                        kInitialPhases[static_cast<size_t> (index)]));
        slots.push_back (std::make_unique<AudioEngine::Slot> (index));
        slots.back()->setSource (sources.back().get());
        slotPointers.push_back (slots.back().get());
    }

    AudioEngine::SlotSwitcher switcher (fadeMilliseconds);
    switcher.setSlots (slotPointers);
    switcher.prepare (kSampleRate, kBlockSize, kNumChannels);

    AudioEngine::Bus bus;
    bus.prepare (kSampleRate, kBlockSize, kNumChannels);

    juce::AudioBuffer<float> block (kNumChannels, kBlockSize);
    juce::MidiBuffer midi;

    switcher.armSlot (0);

    for (int preRollBlock = 0; preRollBlock < kPreRollBlocks; ++preRollBlock)
    {
        bus.beginBlock (kBlockSize);

        if (switcher.process (kBlockSize, midi))
            bus.add (switcher.getOutputBuffer(), kBlockSize);

        bus.finish (block, kBlockSize);

        for (int frame = 0; frame < kBlockSize; ++frame)
            result.master.push_back (block.getSample (0, frame));
    }

    std::mt19937 rng (kSeed);
    std::uniform_int_distribution<int> gapDistribution (kMinimumGapSamples, kMaximumGapSamples);
    std::uniform_int_distribution<int> slotDistribution (0, kNumSlots - 1);

    std::vector<int> eventPositions;
    eventPositions.reserve (kSwitchCount);

    int position = gapDistribution (rng);

    for (int event = 0; event < kSwitchCount; ++event)
    {
        eventPositions.push_back (position);
        position += gapDistribution (rng);
    }

    const int totalBlocks = (eventPositions.back() + kTailSamples + kBlockSize - 1) / kBlockSize;

    int currentSlot = 0;
    std::size_t nextEvent = 0;

    for (int blockIndex = 0; blockIndex < totalBlocks; ++blockIndex)
    {
        const int blockStart = blockIndex * kBlockSize;

        if (nextEvent < eventPositions.size()
            && eventPositions[nextEvent] < blockStart + kBlockSize)
        {
            int target = currentSlot;

            do
            {
                target = slotDistribution (rng);
            }
            while (target == currentSlot);

            switcher.armSlot (target);
            currentSlot = target;

            ++result.switchesIssued;
            ++result.switchesPerSlot[static_cast<size_t> (target)];
            result.switchSamples.push_back (kPreRollSamples + blockStart);
            ++nextEvent;
        }

        bus.beginBlock (kBlockSize);

        if (switcher.process (kBlockSize, midi))
            bus.add (switcher.getOutputBuffer(), kBlockSize);

        bus.finish (block, kBlockSize);

        if (bus.getLimiter().getCurrentGain() < 1.0f)
            result.limiterStayedUnity = false;

        for (int frame = 0; frame < kBlockSize; ++frame)
        {
            const auto sample = block.getSample (0, frame);
            result.peak = juce::jmax (result.peak, static_cast<double> (std::abs (sample)));
            result.master.push_back (sample);
        }
    }

    return result;
}

}
