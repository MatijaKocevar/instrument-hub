#include "Session.h"

#include "audio/RtGuard.h"

#include <algorithm>

namespace ihub
{

bool Session::pushMidi (const juce::MidiMessage& message) noexcept
{
    return midi.push (message);
}

void Session::renderInternal (int numSamples, juce::MidiBuffer& midiBuffer) noexcept
{
    const int samples = juce::jmin (numSamples, maximumBlockSize);

    if (samples <= 0 || engine == nullptr || switcher == nullptr)
        return;

    midi.drain (midiBuffer);

    auto& bus = engine->getBus();

    bus.beginBlock (samples);

    if (switcher->process (samples, midiBuffer))
        bus.add (switcher->getOutputBuffer(), samples);

    bus.finish (renderBuffer, samples);

    transportControl.process (samples);

    meters.update (*engine, *switcher, renderBuffer, samples);

    fxChain.noteBlockRendered();
}

void Session::render (juce::AudioBuffer<float>& output,
                      juce::MidiBuffer& midiBuffer,
                      int numSamples) noexcept
{
    const AudioEngine::RtGuard::Scope rtScope;

    if (! prepared || engine == nullptr || numSamples <= 0)
    {
        output.clear();
        return;
    }

    const int samples = juce::jmin (numSamples, maximumBlockSize);
    renderInternal (samples, midiBuffer);

    const int copyChannels = juce::jmin (numChannels, output.getNumChannels());

    for (int channel = 0; channel < copyChannels; ++channel)
        output.copyFrom (channel, 0, renderBuffer, channel, 0, samples);

    for (int channel = copyChannels; channel < output.getNumChannels(); ++channel)
        output.clear (channel, 0, numSamples);

    if (numSamples > samples)
        for (int channel = 0; channel < copyChannels; ++channel)
            output.clear (channel, samples, numSamples - samples);
}

void Session::copyToOutput (float* const* outputChannelData,
                            int numOutputChannels,
                            int numSamples) noexcept
{
    const int copyChannels = juce::jmin (numChannels, numOutputChannels);

    for (int channel = 0; channel < copyChannels; ++channel)
        if (outputChannelData[channel] != nullptr)
            juce::FloatVectorOperations::copy (outputChannelData[channel],
                                              renderBuffer.getReadPointer (channel),
                                              numSamples);

    for (int channel = copyChannels; channel < numOutputChannels; ++channel)
        if (outputChannelData[channel] != nullptr)
            juce::FloatVectorOperations::clear (outputChannelData[channel], numSamples);
}

void Session::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    if (device == nullptr || engine == nullptr)
        return;

    const double deviceSampleRate = device->getCurrentSampleRate();
    const int deviceBlockSize = device->getCurrentBufferSizeSamples();

    if (deviceSampleRate <= 0.0 || deviceBlockSize <= 0)
        return;

    const bool needsPrepare = ! prepared
                              || std::abs (deviceSampleRate - sampleRate) > 0.5
                              || deviceBlockSize > maximumBlockSize;

    if (needsPrepare)
    {
        prepare (deviceSampleRate,
                 juce::jmax (deviceBlockSize, maximumBlockSize),
                 numChannels);
    }
    else
    {
        engine->getTimingMonitor().prepare (deviceSampleRate, deviceBlockSize);
    }
}

void Session::audioDeviceStopped()
{
}

void Session::audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                                int numInputChannels,
                                                float* const* outputChannelData,
                                                int numOutputChannels,
                                                int numSamples,
                                                const juce::AudioIODeviceCallbackContext&)
{
    const AudioEngine::RtGuard::Scope rtScope;

    if (engine == nullptr)
    {
        if (outputChannelData != nullptr)
            for (int channel = 0; channel < numOutputChannels; ++channel)
                if (outputChannelData[channel] != nullptr)
                    juce::FloatVectorOperations::clear (outputChannelData[channel], numSamples);

        return;
    }

    const auto timingToken = engine->getTimingMonitor().beginCallback();

    if (outputChannelData == nullptr || ! prepared)
    {
        for (int channel = 0; channel < numOutputChannels; ++channel)
            if (outputChannelData[channel] != nullptr)
                juce::FloatVectorOperations::clear (outputChannelData[channel], numSamples);

        engine->getTimingMonitor().endCallback (timingToken, 0);
        return;
    }

    const int samples = juce::jmin (numSamples, maximumBlockSize);

    for (int slot = 0; slot < engine->getNumSlots(); ++slot)
        engine->getSlot (slot).feedDeviceInput (inputChannelData, numInputChannels);

    auto& blockMidi = midi.block();
    blockMidi.clear();
    renderInternal (samples, blockMidi);
    copyToOutput (outputChannelData, numOutputChannels, samples);

    if (numSamples > samples)
        for (int channel = 0; channel < numOutputChannels; ++channel)
            if (outputChannelData[channel] != nullptr)
                juce::FloatVectorOperations::clear (outputChannelData[channel] + samples,
                                                    numSamples - samples);

    engine->getTimingMonitor().endCallback (timingToken, samples);
}

}
