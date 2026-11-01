#include "Scheduling.h"
#include "Fixtures.h"

#include <iostream>
#include <memory>

namespace AudioEngineTest
{

void runOnlyActiveAndFadingSlotsProcess()
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

    for (int i = 0; i < 8; ++i)
        engine.processBlock (output, midi);

    check (first->processCalls.load() == 0
               && second->processCalls.load() == 0
               && third->processCalls.load() == 0,
           "idle slots are never processed");
    check (output.getMagnitude (0, blockSize) <= 0.0f, "idle graph renders silence");

    engine.getSlot (0).activate();

    engine.processBlock (output, midi);
    check (engine.getSlot (0).getState() == AudioEngine::SlotState::fadingIn,
           "slot reports fading-in during the ramp");
    check (engine.getSlot (0).isProcessing(), "a fading slot processes audio");

    engine.processBlock (output, midi);
    engine.processBlock (output, midi);

    check (engine.getSlot (0).getState() == AudioEngine::SlotState::active,
           "slot reaches active after the fade");

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

}
