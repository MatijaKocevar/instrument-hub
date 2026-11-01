#include "Choke.h"

#include "../TestSupport.h"
#include "Constants.h"
#include "Fixtures.h"

#include <cmath>

namespace VoiceTest
{

using SamplerTest::check;
using SamplerTest::number;

void runChokeChecks (Rig& rig)
{
    auto& instrument = rig.instrument;
    instrument.reset();
    instrument.setChokeGroup (46, 1);
    instrument.setChokeGroup (42, 1);

    instrument.noteOn (1, 46, 1.0f);
    const auto openOnly = render (instrument, 128);
    check (peakLevel (openOnly) > 0.1f,
           "the open hat is audible before the choke (peak "
               + number (peakLevel (openOnly)) + ")");
    check (instrument.getActiveVoiceCount() == 1, "one open-hat voice is active");

    instrument.noteOn (1, 42, 1.0f);
    const int windowSamples = (int) std::lround (0.005 * engineRate);
    const auto window = render (instrument, windowSamples);

    const int fadeSamples = (int) std::ceil (
        (double) instrument.getChokeFadeMilliseconds() * 0.001 * engineRate);
    const int tailStart = juce::jmin (windowSamples - 1, fadeSamples);
    const int tailLength = windowSamples - tailStart;
    const auto tailPeak = peakLevel (window, tailStart, tailLength);

    check (instrument.getActiveVoiceCount() == 1,
           "the open-hat voice is gone after the choke (only the silent "
           "closed-hat voice remains)");
    check (tailPeak <= 1.0e-6f,
           "the open hat is silent by 5 ms (tail peak " + number (tailPeak) + ")");

    const auto afterWindow = render (instrument, 128);
    check (peakLevel (afterWindow) <= 1.0e-6f,
           "the choked voice stays silent after the window");
}

}
