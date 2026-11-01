#include "Pool.h"

#include "../TestSupport.h"
#include "Fixtures.h"

#include <string>

namespace VoiceTest
{

using SamplerTest::check;
using SamplerTest::number;

void runPoolChecks (Rig& rig)
{
    auto& instrument = rig.instrument;
    instrument.reset();

    for (int i = 0; i < 16; ++i)
        instrument.noteOn (1, 36, 1.0f);

    check (instrument.getPendingEventCount() == 16, "16 note-ons are queued");
    const auto block = render (instrument, 128);

    check (instrument.getActiveVoiceCount() == 16,
           "16 simultaneous hits use 16 voices (got "
               + std::to_string (instrument.getActiveVoiceCount()) + ")");
    check (instrument.getStartedVoiceCount() == 16, "all 16 hits started");
    check (instrument.getStolenVoiceCount() == 0, "no voice was stolen");
    check (instrument.getDroppedEventCount() == 0, "no event was dropped");
    check (peakLevel (block) > 0.1f,
           "the 16-voice block is audible (peak " + number (peakLevel (block)) + ")");

    for (int i = 0; i < 16; ++i)
        instrument.noteOff (1, 36);

    const auto afterNoteOff = render (instrument, 128);
    check (instrument.getActiveVoiceCount() == 16,
           "one-shot voices survive note-off (got "
               + std::to_string (instrument.getActiveVoiceCount()) + ")");
    check (peakLevel (afterNoteOff) > 0.1f, "the one-shot voices keep sounding");
}

}
