#include "DirectMonitor.h"
#include "Fixtures.h"
#include "TestSupport.h"

#include "audio/Slot.h"
#include "instruments/AudioInputInstrument.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace AudioInputTest
{

void runDirectMonitorBypassesFxChain()
{
    std::cout << "\n-- direct monitoring bypasses the FX chain --" << std::endl;

    AudioInputInstrument input;
    input.prepare (kSampleRate, kBlockSize);
    input.setLatencySamples (96);

    GainFxProcessor fx (0.5f, 240);

    AudioEngine::Slot slot (0);
    slot.setSource (&input);
    slot.prepare (kSampleRate, kBlockSize, 2);
    check (slot.getFxChain().add (&fx) == 0, "the gain effect is added to the slot chain");
    slot.activate();

    juce::AudioBuffer<float> source (1, kBlockSize);
    fillSine (source, 0);

    juce::MidiBuffer midi;

    const float* channels[] = { source.getReadPointer (0) };

    for (int block = 0; block < kFadeBlocks; ++block)
    {
        input.setInputBlock (channels, 1);
        slot.process (kBlockSize, midi);
    }

    const int callsAfterFade = fx.getProcessCalls();

    check (callsAfterFade >= kFadeBlocks, "the chain runs every block in normal mode");
    check (slot.latencySamples() == 96 + 240,
           "normal mode reports input plus chain latency");

    double fxDifference = 0.0;
    const auto& processed = slot.getOutputBuffer();

    for (int channel = 0; channel < 2; ++channel)
        for (int i = 0; i < kBlockSize; ++i)
            fxDifference = std::max (
                fxDifference,
                std::abs (static_cast<double> (processed.getSample (channel, i))
                          - 0.5 * static_cast<double> (source.getSample (0, i))));

    check (fxDifference < 1.0e-7, "input -> FX -> bus applies the chain gain");

    input.setDirectMonitor (true);
    check (input.isDirectMonitor(), "direct monitoring is latched");
    check (input.bypassesFxChain(), "direct monitoring asks the slot to bypass the chain");

    input.setInputBlock (channels, 1);
    check (slot.process (kBlockSize, midi), "the slot still renders the input while monitoring");

    check (fx.getProcessCalls() == callsAfterFade,
           "the bypassed chain's processBlock is never called");
    check (slot.latencySamples() == 96,
           "direct mode reports only the input path's own latency");

    bool bitIdentical = true;
    const auto& monitored = slot.getOutputBuffer();

    for (int channel = 0; channel < 2 && bitIdentical; ++channel)
        for (int i = 0; i < kBlockSize; ++i)
            if (! juce::exactlyEqual (monitored.getSample (channel, i),
                                      source.getSample (0, i)))
            {
                bitIdentical = false;
                break;
            }

    check (bitIdentical, "direct-monitor output is the input, bit-exact");

    input.setDirectMonitor (false);
    input.setInputBlock (channels, 1);
    slot.process (kBlockSize, midi);

    check (fx.getProcessCalls() == callsAfterFade + 1,
           "leaving direct mode runs the chain again on the next block");
    check (slot.latencySamples() == 96 + 240,
           "latency returns to input plus chain");
}

} // namespace AudioInputTest
