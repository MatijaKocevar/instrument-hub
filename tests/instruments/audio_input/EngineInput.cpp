#include "EngineInput.h"
#include "Fixtures.h"
#include "TestSupport.h"

#include "audio/AudioEngine.h"
#include "instruments/AudioInputInstrument.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace AudioInputTest
{

void runEngineFeedsDeviceInput()
{
    std::cout << "\n-- the engine feeds the device input to an audio-input slot --" << std::endl;

    using Engine = AudioEngine::AudioEngine;

    Engine engine (2, 2);
    engine.prepare (kSampleRate, kBlockSize, 2);

    AudioInputInstrument input;
    input.setGain (0.25f);

    AudioEngine::Slot& slot = engine.getSlot (0);
    slot.setSource (&input);
    slot.activate();

    juce::AudioBuffer<float> deviceInput (1, kBlockSize);
    fillSine (deviceInput, 0);

    juce::AudioBuffer<float> deviceOutput (2, kBlockSize);
    float* outputChannels[] = { deviceOutput.getWritePointer (0),
                                deviceOutput.getWritePointer (1) };
    const float* inputChannels[] = { deviceInput.getReadPointer (0) };

    for (int block = 0; block < kFadeBlocks; ++block)
        engine.audioDeviceIOCallbackWithContext (inputChannels, 1,
                                                 outputChannels, 2,
                                                 kBlockSize, {});

    check (slot.getState() == AudioEngine::SlotState::active,
           "the engine-driven audio-input slot reaches active");

    double maxDifference = 0.0;

    for (int channel = 0; channel < 2; ++channel)
        for (int i = 0; i < kBlockSize; ++i)
            maxDifference = std::max (
                maxDifference,
                std::abs (static_cast<double> (deviceOutput.getSample (channel, i))
                          - 0.25 * static_cast<double> (deviceInput.getSample (0, i))));

    check (maxDifference < 1.0e-7,
           "the device callback path renders the instrument's gained input");
}

} // namespace AudioInputTest
