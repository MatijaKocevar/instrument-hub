#include "Gate.h"

#include "../TestSupport.h"
#include "Constants.h"
#include "Fixtures.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>
#include <string>

namespace VoiceTest
{

using SamplerTest::check;
using ihub::sampler::SamplerInstrument;

void runGateChecks (Rig& rig)
{
    auto& instrument = rig.instrument;
    instrument.reset();
    instrument.setPadMode (50, SamplerInstrument::PadMode::gate);

    instrument.noteOn (1, 50, 1.0f);
    const auto held = render (instrument, 128);
    check (peakLevel (held) > 0.1f, "the gate pad sounds while held");
    check (instrument.getActiveVoiceCount() == 1, "the gate voice is active");

    instrument.noteOff (1, 50);
    const auto released = render (instrument, (int) std::lround (0.020 * engineRate));
    check (instrument.getActiveVoiceCount() == 0,
           "the gate voice stops after note-off (got "
               + std::to_string (instrument.getActiveVoiceCount()) + ")");
    check (peakLevel (released, 0, released.getNumSamples() / 2) > 0.0f,
           "the gate release is a fade, not an instant cut");
    check (peakLevel (released, released.getNumSamples() / 2,
                      released.getNumSamples() / 2) <= 1.0e-6f,
           "the gate voice is silent by the end of its release");

    instrument.setPadMode (50, SamplerInstrument::PadMode::oneShot);
    instrument.reset();
    instrument.noteOn (1, 36, 1.0f);
    render (instrument, 64);
    instrument.noteOff (1, 36);
    const auto stillPlaying = render (instrument, 64);
    check (instrument.getActiveVoiceCount() == 1 && peakLevel (stillPlaying) > 0.1f,
           "a one-shot ignores note-off and keeps playing");

    instrument.reset();
    instrument.setPadMode (50, SamplerInstrument::PadMode::gate);

    juce::AudioBuffer<float> buffer (2, 128);
    buffer.clear();
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 50, (juce::uint8) 100), 32);

    instrument.process (buffer, midi);

    check (peakLevel (buffer, 0, 32) <= 1.0e-9f,
           "the block is silent before the MIDI event");
    check (peakLevel (buffer, 32, 96) > 0.1f,
           "the MIDI note-on starts at its sample offset");
    check (instrument.getActiveVoiceCount() == 1, "the MIDI event starts a voice");
}

}
