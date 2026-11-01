#include "Setup.h"

#include "../Fixtures.h"
#include "../TestSupport.h"
#include "Constants.h"
#include "Fixtures.h"

#include "sampler/KitLoader.h"

#include <cmath>
#include <string>

namespace VoiceTest
{

using SamplerTest::check;
using SamplerTest::saveFloatWav;
using ihub::sampler::KitLoader;

bool runSetupChecks (Rig& rig)
{
    const int fixtureFrames = (int) engineRate * fixtureSeconds;

    saveFloatWav (rig.directory.getChildFile ("kick.wav"),
                  SamplerTest::makeSine ((int) engineRate / 2, engineRate, 220.0),
                  engineRate);

    juce::AudioBuffer<float> silence (1, (int) engineRate / 2);
    silence.clear();
    saveFloatWav (rig.directory.getChildFile ("closed-hat.wav"), silence, engineRate);

    saveFloatWav (rig.directory.getChildFile ("open-hat.wav"),
                  SamplerTest::makeSine (fixtureFrames, engineRate, 440.0, 0.5),
                  engineRate);
    saveFloatWav (rig.directory.getChildFile ("gate.wav"),
                  SamplerTest::makeSine (fixtureFrames, engineRate, 330.0, 0.5),
                  engineRate);

    juce::String error;
    check (KitLoader::loadFromJson (juce::String (fixtureJson()), rig.directory,
                                    engineRate, rig.kit, error),
           "pad test kit loads (" + error.toStdString() + ")");
    check (rig.kit.getKit().pads.size() == 4, "four pads are loaded");

    rig.instrument.setKit (&rig.kit);
    rig.instrument.prepare (engineRate, maximumBlock);

    check (rig.instrument.kind() == ihub::Instrument::Kind::Sampler,
           "kind() reports the sampler");
    check (rig.instrument.latencySamples() == 0, "latency is zero");
    check (rig.instrument.getVoiceCapacity() == 64
               && rig.instrument.getVoiceCapacity() > 16,
           "the voice pool holds 64 voices, comfortably above 16");
    check (rig.instrument.getPadMode (36) == ihub::sampler::SamplerInstrument::PadMode::oneShot,
           "pads default to one-shot");
    check (rig.instrument.getChokeGroup (36) == ihub::sampler::SamplerInstrument::noChokeGroup,
           "pads default to no choke group");
    check (std::abs (rig.instrument.getChokeFadeMilliseconds() - 2.0f) < 1.0e-6f,
           "the default choke fade is 2 ms");

    return rig.kit.isValid();
}

} // namespace VoiceTest
