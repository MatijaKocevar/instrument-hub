#include "Channels.h"
#include "Fixtures.h"

#include "instruments/AudioInputInstrument.h"

#include <cmath>
#include <iostream>

namespace AudioInputTest
{

void runChannelSelectionAndMissingInput()
{
    std::cout << "\n-- channel selection and missing input --" << std::endl;

    AudioInputInstrument input;
    input.prepare (kSampleRate, kBlockSize);

    juce::AudioBuffer<float> twoChannels (2, kBlockSize);
    twoChannels.clear();
    juce::FloatVectorOperations::fill (twoChannels.getWritePointer (0), 0.1f, kBlockSize);
    juce::FloatVectorOperations::fill (twoChannels.getWritePointer (1), 0.2f, kBlockSize);

    const float* channels[] = { twoChannels.getReadPointer (0),
                                twoChannels.getReadPointer (1) };

    juce::AudioBuffer<float> rendered (2, kBlockSize);
    juce::MidiBuffer midi;

    input.setInputChannel (1);
    input.setInputBlock (channels, 2);
    input.process (rendered, midi);

    check (std::abs (rendered.getSample (0, 0) - 0.2f) <= 0.0f
               && std::abs (rendered.getSample (1, 0) - 0.2f) <= 0.0f,
           "the selected interface channel is copied to the output");

    input.setInputChannel (5);
    input.setInputBlock (channels, 2);
    input.process (rendered, midi);

    check (rendered.getMagnitude (0, kBlockSize) <= 0.0f,
           "an out-of-range channel renders silence");

    rendered.clear();
    rendered.getWritePointer (0)[0] = 0.5f;
    input.process (rendered, midi);

    check (rendered.getMagnitude (0, kBlockSize) <= 0.0f,
           "no input block renders silence (the hand-off is one block only)");
}

}
