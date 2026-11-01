#include "Realtime.h"

#include "../TestSupport.h"
#include "Constants.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <string>

namespace VoiceTest
{

using SamplerTest::check;

void runRealtimeChecks (Rig& rig)
{
    auto& instrument = rig.instrument;
    instrument.reset();
    instrument.noteOn (1, 46, 1.0f);
    instrument.noteOn (1, 42, 1.0f);

    juce::AudioBuffer<float> buffer (2, maximumBlock);
    juce::MidiBuffer midi;

    allocCount::total.store (0);
    allocCount::enabled.store (true);

    for (int i = 0; i < 1000; ++i)
    {
        if ((i % 16) == 0)
        {
            instrument.noteOn (1, 36, 0.8f);
            instrument.noteOn (1, 46, 0.9f);
        }

        buffer.clear();
        instrument.process (buffer, midi);
    }

    allocCount::enabled.store (false);

    check (allocCount::total.load() == 0,
           "process() allocates nothing (" + std::to_string (allocCount::total.load())
               + " allocations)");
}

} // namespace VoiceTest
