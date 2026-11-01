#include "Stealing.h"

#include "../TestSupport.h"
#include "Fixtures.h"

#include <string>

namespace VoiceTest
{

using SamplerTest::check;
using ihub::sampler::SamplerInstrument;

void runStealingChecks (Rig& rig)
{
    auto& instrument = rig.instrument;
    instrument.reset();

    const int startedBefore = instrument.getStartedVoiceCount();
    const int extra = 4;

    for (int i = 0; i < SamplerInstrument::maxVoices + extra; ++i)
        instrument.noteOn (1, 36, 1.0f);

    render (instrument, 128);

    check (instrument.getActiveVoiceCount() == SamplerInstrument::maxVoices,
           "the active voice count never exceeds the pool (got "
               + std::to_string (instrument.getActiveVoiceCount()) + ")");
    check (instrument.getStolenVoiceCount() == extra,
           "excess hits steal the oldest voices (got "
               + std::to_string (instrument.getStolenVoiceCount()) + ")");
    check (instrument.getStartedVoiceCount() - startedBefore
               == SamplerInstrument::maxVoices + extra,
           "every hit was started, either in a free or a stolen voice");
}

}
