#include "TestPluginProcessor.h"

#include <cmath>

namespace
{
constexpr auto stateTypeName = "TestPluginState";

float readParameter (juce::AudioProcessorValueTreeState& state, const char* id) noexcept
{
    if (auto* value = state.getRawParameterValue (id))
        return value->load (std::memory_order_relaxed);

    return 0.0f;
}
} // namespace

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
TestPluginAudioProcessor::createParameterLayout()
{
    using namespace juce;

    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { gainParamID, 1 }, "Gain",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f,
        AudioParameterFloatAttributes().withLabel ("x")));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { frequencyParamID, 1 }, "Frequency",
        NormalisableRange<float> (20.0f, 20000.0f, 0.01f), 440.0f,
        AudioParameterFloatAttributes().withLabel ("Hz")));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { latencyParamID, 1 }, "Latency",
        NormalisableRange<float> (0.0f, maxLatencyMs, 0.01f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("ms")));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { sineLevelParamID, 1 }, "Sine Level",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("x")));

    // Deliberately unused by processBlock. It exists so T022 can prove that
    // a non-default, DSP-irrelevant value survives save/load.
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { stateMarkerParamID, 1 }, "State Marker",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));

    // When on, the sine generator only sounds while a note is held. The
    // T128 session test uses this to prove live MIDI reaches an armed plugin
    // and that a note-off silences it. Default off keeps every existing test's
    // continuous-tone behaviour.
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { midiGateParamID, 1 }, "MIDI Gate",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));

    return layout;
}

//==============================================================================
TestPluginAudioProcessor::TestPluginAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, stateTypeName, createParameterLayout())
{
}

//==============================================================================
void TestPluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    heldNotes.fill (false);

    maxDelaySamples = static_cast<int> (std::ceil (maxLatencyMs * 0.001 * currentSampleRate)) + 2;

    const auto channels = juce::jmax (1, getTotalNumOutputChannels());
    const auto delayLength = maxDelaySamples + juce::jmax (1, samplesPerBlock) + 2;

    delayBuffer.setSize (channels, delayLength, false, true, false);
    delayBuffer.clear();
    delayWritePosition = 0;

    phase = 0.0;

    reportedLatency = static_cast<int> (std::lround (
        readParameter (apvts, latencyParamID) * 0.001 * currentSampleRate));
    reportedLatency = juce::jlimit (0, maxDelaySamples, reportedLatency);
    setLatencySamples (reportedLatency);
}

void TestPluginAudioProcessor::releaseResources()
{
    delayBuffer.setSize (0, 0);
    delayWritePosition = 0;
    reportedLatency = 0;
    setLatencySamples (0);
}

bool TestPluginAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto output = layouts.getMainOutputChannelSet();

    if (output != juce::AudioChannelSet::stereo() && output != juce::AudioChannelSet::mono())
        return false;

    const auto input = layouts.getMainInputChannelSet();

    // The input bus is optional, but when it is present it must match the
    // output layout. That gives the host 2-in/2-out, 1-in/1-out and
    // 0-in/2-out (generator) modes.
    return input.isDisabled() || input == output;
}

void TestPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numSamples = buffer.getNumSamples();
    const auto numInputs = getTotalNumInputChannels();
    const auto numOutputs = getTotalNumOutputChannels();

    if (numSamples <= 0 || numOutputs <= 0)
        return;

    for (int channel = numInputs; channel < numOutputs; ++channel)
        buffer.clear (channel, 0, numSamples);

    const auto gain = readParameter (apvts, gainParamID);
    const auto frequency = readParameter (apvts, frequencyParamID);
    const auto sineLevel = readParameter (apvts, sineLevelParamID);
    const auto latencyMs = readParameter (apvts, latencyParamID);
    const auto midiGate = readParameter (apvts, midiGateParamID) >= 0.5f;

    if (midiGate)
    {
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
    }

    bool anyHeld = false;

    if (midiGate)
        for (const auto held : heldNotes)
            anyHeld = anyHeld || held;

    const auto gate = midiGate && ! anyHeld ? 0.0f : 1.0f;

    const auto wantedLatency = juce::jlimit (
        0, maxDelaySamples,
        static_cast<int> (std::lround (latencyMs * 0.001 * currentSampleRate)));

    if (wantedLatency != reportedLatency)
    {
        reportedLatency = wantedLatency;
        setLatencySamples (wantedLatency);
    }

    const auto phaseIncrement = juce::MathConstants<double>::twoPi
                              * static_cast<double> (frequency) / currentSampleRate;

    // Gain and the optional sine generator. The generator is added after the
    // gain so a plugin instance can be used as a pure gain stage (sine level 0)
    // or as a tone source for slot/summing tests.
    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto sine = static_cast<float> (std::sin (phase)) * sineLevel * gate;

        phase += phaseIncrement;

        if (phase >= juce::MathConstants<double>::twoPi)
            phase -= juce::MathConstants<double>::twoPi;

        for (int channel = 0; channel < numOutputs; ++channel)
        {
            const auto input = channel < numInputs ? buffer.getSample (channel, sample) : 0.0f;
            buffer.setSample (channel, sample, input * gain + sine);
        }
    }

    // Artificial latency. The delay line is pre-allocated in prepareToPlay();
    // changing the parameter while running only moves the read cursor.
    if (wantedLatency > 0 && delayBuffer.getNumSamples() > wantedLatency)
    {
        const auto bufferLength = delayBuffer.getNumSamples();
        const auto delayChannels = juce::jmin (delayBuffer.getNumChannels(), numOutputs);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const auto writeIndex = (delayWritePosition + sample) % bufferLength;
            const auto readIndex = (writeIndex - wantedLatency + bufferLength) % bufferLength;

            for (int channel = 0; channel < delayChannels; ++channel)
            {
                const auto input = buffer.getSample (channel, sample);
                const auto delayed = delayBuffer.getSample (channel, readIndex);

                delayBuffer.setSample (channel, writeIndex, input);
                buffer.setSample (channel, sample, delayed);
            }
        }

        delayWritePosition = (delayWritePosition + numSamples) % bufferLength;
    }
}

//==============================================================================
juce::AudioProcessorEditor* TestPluginAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

//==============================================================================
void TestPluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();

    if (const auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void TestPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}
