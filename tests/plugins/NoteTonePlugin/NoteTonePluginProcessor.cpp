#include "NoteTonePluginProcessor.h"

#include <cmath>

NoteTonePluginProcessor::NoteTonePluginProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

void NoteTonePluginProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    phase = 0.0;
    heldNotes.fill (false);
}

bool NoteTonePluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto output = layouts.getMainOutputChannelSet();
    return output == juce::AudioChannelSet::mono()
        || output == juce::AudioChannelSet::stereo();
}

void NoteTonePluginProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numSamples = buffer.getNumSamples();
    const auto numOutputs = getTotalNumOutputChannels();

    if (numSamples <= 0 || numOutputs <= 0)
        return;

    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();

        if (message.isNoteOn())
            heldNotes[static_cast<std::size_t> (
                juce::jlimit (0, 127, message.getNoteNumber()))] = true;
        else if (message.isNoteOff())
            heldNotes[static_cast<std::size_t> (
                juce::jlimit (0, 127, message.getNoteNumber()))] = false;
        else if (message.isAllNotesOff() || message.isAllSoundOff())
            heldNotes.fill (false);
    }

    bool anyHeld = false;

    for (const auto held : heldNotes)
        anyHeld = anyHeld || held;

    const auto increment = juce::MathConstants<double>::twoPi
                         * static_cast<double> (toneFrequency) / currentSampleRate;

    for (int frame = 0; frame < numSamples; ++frame)
    {
        const auto value = anyHeld
                               ? static_cast<float> (std::sin (phase)) * toneAmplitude
                               : 0.0f;

        phase += increment;

        if (phase >= juce::MathConstants<double>::twoPi)
            phase -= juce::MathConstants<double>::twoPi;

        for (int channel = 0; channel < numOutputs; ++channel)
            buffer.setSample (channel, frame, value);
    }
}
