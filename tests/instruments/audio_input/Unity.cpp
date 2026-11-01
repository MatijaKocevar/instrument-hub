#include "Unity.h"
#include "Fixtures.h"
#include "TestSupport.h"

#include "audio/AudioEngine.h"
#include "audio/Bus.h"
#include "audio/Slot.h"
#include "instruments/AudioInputInstrument.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace AudioInputTest
{

void runUnityGainIntoBus()
{
    std::cout << "\n-- unity gain through slot and master bus --" << std::endl;

    AudioInputInstrument input;
    input.setInputChannel (0);
    input.prepare (kSampleRate, kBlockSize);

    juce::AudioBuffer<float> source (1, kBlockSize);
    fillSine (source, 0);

    AudioEngine::Bus bus;
    bus.prepare (kSampleRate, kBlockSize, 2);

    AudioEngine::Slot slot (0);
    slot.setSource (&input);
    slot.prepare (kSampleRate, kBlockSize, 2);
    slot.activate();

    juce::AudioBuffer<float> rendered (2, kBlockSize);
    juce::MidiBuffer midi;

    double maxDifference = 0.0;
    int processedBlocks = 0;

    for (int blockIndex = 0; blockIndex < kFadeBlocks; ++blockIndex)
    {
        const float* channels[] = { source.getReadPointer (0) };
        input.setInputBlock (channels, 1);

        if (slot.process (kBlockSize, midi))
            ++processedBlocks;

        bus.beginBlock (kBlockSize);
        bus.add (slot.getOutputBuffer(), kBlockSize);
        bus.finish (rendered, kBlockSize);

        if (blockIndex == kFadeBlocks - 1)
        {
            for (int channel = 0; channel < 2; ++channel)
                for (int i = 0; i < kBlockSize; ++i)
                    maxDifference = std::max (maxDifference,
                                              std::abs (static_cast<double> (
                                                            rendered.getSample (channel, i))
                                                        - static_cast<double> (
                                                            source.getSample (0, i))));
        }
    }

    check (processedBlocks == kFadeBlocks,
           "slot processes the audio input through the fade");
    check (slot.getState() == AudioEngine::SlotState::active,
           "slot is active after its fade ramp");
    check (maxDifference <= 0.0,
           "master bus output is bit-identical to the input at unity gain");
}

} // namespace AudioInputTest
