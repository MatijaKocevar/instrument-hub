#include "EmptyChain.h"
#include "Fixtures.h"

#include "audio/Bus.h"
#include "audio/FxChain.h"

#include <cstring>
#include <iostream>

namespace FxChainTest
{

void runEmptyChainIsBitExact()
{
    std::cout << "\n-- empty chain is a bit-exact passthrough --" << std::endl;

    AudioEngine::FxChain chain;
    chain.prepare (kSampleRate, kBlockSize, kNumChannels);

    check (chain.isEmpty(), "a fresh chain is empty");
    check (chain.size() == 0, "size() is zero");
    check (chain.latencySamples() == 0, "an empty chain reports zero latency");

    juce::AudioBuffer<float> block (kNumChannels, kBlockSize);
    fillPattern (block);

    juce::AudioBuffer<float> reference;
    reference.makeCopyOf (block);

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 0);

    const int midiEventsBefore = midi.getNumEvents();

    chain.process (block, midi);

    check (buffersBitEqual (block, reference),
           "FxChain::process leaves the buffer bit-identical when empty");
    check (midi.getNumEvents() == midiEventsBefore,
           "FxChain::process leaves the MIDI buffer untouched when empty");
}

void runEmptyChainThroughSlot()
{
    std::cout << "\n-- empty chain through the Slot path --" << std::endl;

    PatternSource source;
    AudioEngine::Slot slot (0);

    check (slot.getFxChain().isEmpty(), "a new slot starts with an empty FX chain");

    slot.setSource (&source);
    slot.prepare (kSampleRate, kBlockSize, kNumChannels);
    slot.activate();

    juce::MidiBuffer midi;
    bool processedEveryBlock = true;

    for (int block = 0; block < kFadeBlocks; ++block)
        if (! slot.process (kBlockSize, midi))
            processedEveryBlock = false;

    check (processedEveryBlock, "the slot processes every block during its fade-in");
    check (slot.getState() == AudioEngine::SlotState::active,
           "the slot is active after the fade ramp");

    const auto& output = slot.getOutputBuffer();
    const auto& pattern = source.getPattern();

    bool identical = output.getNumChannels() == kNumChannels
                     && output.getNumSamples() == kBlockSize;

    if (identical)
        for (int channel = 0; channel < kNumChannels; ++channel)
            if (std::memcmp (output.getReadPointer (channel),
                             pattern.data(),
                             static_cast<std::size_t> (kBlockSize) * sizeof (float)) != 0)
                identical = false;

    check (identical,
           "slot output is bit-identical to the source with an empty chain");
    check (slot.latencySamples() == source.latencySamples(),
           "an empty chain adds no latency to the slot total");
}

} // namespace FxChainTest
