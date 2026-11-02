#include "Fixtures.h"

#include "plugins/PluginInstrument.h"

#include "../TestSupport.h"

#include <cmath>

namespace ApiTest::PatchStateCaptureApi
{

namespace
{

float normaliseFrequency (double hz)
{
    return static_cast<float> ((hz - 20.0) / (20000.0 - 20.0));
}

}

juce::File makeTempRoot()
{
    return juce::File::getSpecialLocation (juce::File::tempDirectory)
        .getChildFile ("ihub-t132-"
                       + juce::String::toHexString (
                           juce::Random::getSystemRandom().nextInt64()));
}

juce::MemoryBlock readBytes (const juce::File& file)
{
    juce::MemoryBlock data;
    (void) file.loadFileAsData (data);
    return data;
}

ihub::SessionConfig makeConfig (const juce::String& bundlePath)
{
    ihub::SessionConfig config;
    config.device.sampleRate = kSampleRate;
    config.device.bufferSize = kBlockSize;
    config.device.numOutputChannels = 2;
    config.device.numInputChannels = 2;
    config.fadeMilliseconds = 3.0f;
    config.neighbourCount = 1;
    config.initialSlot = -1;
    config.slots = ihub::Session::makeDefaultSet (ihub::SessionSlotKind::initTone);
    config.slots.resize (kSlots);

    auto& plugin = config.slots[static_cast<std::size_t> (kPluginSlot)];
    plugin.kind = ihub::SessionSlotKind::plugin;
    plugin.path = bundlePath;
    plugin.pluginFormat = "VST3";
    plugin.pluginName = "TestPlugin";
    plugin.instrumentKind = "plugin";
    plugin.name = "Saved Sound";
    plugin.patch = "saved-sound";
    plugin.patchName = plugin.name;

    auto& tone = config.slots[static_cast<std::size_t> (kToneSlot)];
    tone.kind = ihub::SessionSlotKind::initTone;
    tone.frequency = 220.0;
    tone.name = "Tone";
    tone.patch = "tone";
    tone.patchName = tone.name;

    return config;
}

void applyPreset (ihub::Session& session,
                  int slot,
                  double frequency,
                  float stateMarkerValue)
{
    auto* instrument = session.getPluginInstrument (slot);

    check (instrument != nullptr, "slot " + std::to_string (slot) + " has a live plugin");

    if (instrument == nullptr)
        return;

    (void) instrument->setParameterValue ("Sine Level", 1.0f);
    (void) instrument->setParameterValue ("Gain", 0.0f);
    (void) instrument->setParameterValue ("Frequency", normaliseFrequency (frequency));
    (void) instrument->setParameterValue ("State Marker", stateMarkerValue);
}

float stateMarker (ihub::Session& session, int slot)
{
    auto* instrument = session.getPluginInstrument (slot);
    return instrument != nullptr ? instrument->getParameterValue ("State Marker") : -1.0f;
}

double measurePitch (ihub::Session& session)
{
    juce::AudioBuffer<float> buffer (2, kBlockSize);
    juce::MidiBuffer midi;

    for (int block = 0; block < 48; ++block)
    {
        buffer.clear();
        session.render (buffer, midi, kBlockSize);
        juce::Thread::sleep (1);
    }

    int crossings = 0;
    int frames = 0;
    float previous = 0.0f;

    for (int block = 0; block < 192; ++block)
    {
        buffer.clear();
        session.render (buffer, midi, kBlockSize);
        juce::Thread::sleep (1);

        for (int frame = 0; frame < kBlockSize; ++frame)
        {
            const float sample = buffer.getSample (0, frame);

            if (previous <= 0.0f && sample > 0.0f)
                ++crossings;

            previous = sample;
            ++frames;
        }
    }

    return frames > 0 ? static_cast<double> (crossings) * kSampleRate
                            / static_cast<double> (frames)
                      : 0.0;
}

double denormaliseFrequency (float normalised)
{
    return 20.0 + static_cast<double> (normalised) * (20000.0 - 20.0);
}

bool approximately (double value, double expected, double tolerance)
{
    return std::abs (value - expected) <= tolerance;
}

}
