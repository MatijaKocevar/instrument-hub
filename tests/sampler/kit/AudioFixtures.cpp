#include "AudioFixtures.h"

#include "../Fixtures.h"
#include "Constants.h"

#include <juce_audio_formats/juce_audio_formats.h>

namespace KitTest
{

void writeFixtures (const juce::File& directory)
{
    SamplerTest::saveFloatWav (directory.getChildFile ("kick-soft-a.wav"),
                               SamplerTest::makeSine (fixtureFrames, engineRate, 220.0),
                               engineRate);
    SamplerTest::saveFloatWav (directory.getChildFile ("kick-soft-b.wav"),
                               SamplerTest::makeSine (fixtureFrames, engineRate, 330.0),
                               engineRate);
    SamplerTest::saveFloatWav (directory.getChildFile ("kick-hard.wav"),
                               SamplerTest::makeSine (fixtureFrames, engineRate, 880.0),
                               engineRate);
}

} // namespace KitTest
