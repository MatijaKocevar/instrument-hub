#include "AudioEngine.h"
#include "RtGuard.h"

#include <cmath>

namespace AudioEngine
{

AudioEngine::AudioEngine (int numSlots, int numChannelsToUse)
    : numChannels (juce::jmax (1, numChannelsToUse))
{
    const int slotCount = juce::jmax (1, numSlots);

    slots.reserve (static_cast<size_t> (slotCount));

    for (int index = 0; index < slotCount; ++index)
        slots.push_back (std::make_unique<Slot> (index));

    // No samples are needed until prepare() is called.
    renderBuffer.setSize (numChannels, 0, false, true, true);
}

void AudioEngine::prepare (double newSampleRate, int newMaximumBlockSize, int newNumChannels)
{
    jassert (newSampleRate > 0.0);
    jassert (newMaximumBlockSize > 0);
    jassert (newNumChannels > 0);

    sampleRate = newSampleRate;
    maximumBlockSize = juce::jmax (1, newMaximumBlockSize);
    numChannels = juce::jmax (1, newNumChannels);

    renderBuffer.setSize (numChannels, maximumBlockSize, false, true, true);
    renderBuffer.clear();

    bus.prepare (sampleRate, maximumBlockSize, numChannels);
    timingMonitor.prepare (sampleRate, maximumBlockSize);

    for (auto& slot : slots)
        slot->prepare (sampleRate, maximumBlockSize, numChannels);

    deviceMidi.clear();
    prepared = true;
}

void AudioEngine::releaseResources()
{
    prepared = false;
    bus.releaseResources();

    for (auto& slot : slots)
        slot->reset();

    renderBuffer.setSize (0, 0);
}

void AudioEngine::reset() noexcept
{
    bus.reset();

    for (auto& slot : slots)
        slot->reset();

    renderBuffer.clear();
    deviceMidi.clear();
}

Slot& AudioEngine::getSlot (int index) noexcept
{
    jassert (index >= 0 && index < getNumSlots());
    return *slots[static_cast<size_t> (juce::jlimit (0, getNumSlots() - 1, index))];
}

const Slot& AudioEngine::getSlot (int index) const noexcept
{
    jassert (index >= 0 && index < getNumSlots());
    return *slots[static_cast<size_t> (juce::jlimit (0, getNumSlots() - 1, index))];
}

int AudioEngine::getLatencySamples() const noexcept
{
    int latency = 0;

    for (const auto& slot : slots)
        latency = juce::jmax (latency, slot->latencySamples());

    return latency;
}

LatencyResult AudioEngine::measureRoundTripLatency (const LatencyProbeOptions& options)
{
    LatencyProbe probe;
    return probe.measure (options);
}

void AudioEngine::processBlock (juce::AudioBuffer<float>& output, juce::MidiBuffer& midi) noexcept
{
    processBlock (output, midi, output.getNumSamples());
}

void AudioEngine::processBlock (juce::AudioBuffer<float>& output,
                                juce::MidiBuffer& midi,
                                int numSamples) noexcept
{
    // T014: every violation of the audio-thread contract inside this call
    // (allocation, lock or log) aborts in --rt-strict mode.
    const RtGuard::Scope rtScope;

    if (! prepared || numSamples <= 0)
    {
        output.clear();
        return;
    }

    const int samples = juce::jmin (numSamples, maximumBlockSize);

    renderBlock (samples, midi);
    copyToOutput (output, samples);

    // If the caller asked for more than the prepared maximum, keep the tail
    // silent instead of repeating stale audio.
    if (numSamples > samples)
        for (int channel = 0; channel < output.getNumChannels(); ++channel)
            output.clear (channel, samples, numSamples - samples);
}

void AudioEngine::feedDeviceInput (const float* const* channelData,
                                   int newNumChannels) noexcept
{
    // Every slot sees the same pointers; only a source that reports
    // `acceptsDeviceInput()` keeps them, and it forgets them at the end of its
    // next process() call. No copying, no allocation.
    for (auto& slot : slots)
        slot->feedDeviceInput (channelData, newNumChannels);
}

void AudioEngine::renderBlock (int numSamples, juce::MidiBuffer& midi) noexcept
{
    const int samples = juce::jmin (numSamples, maximumBlockSize);

    if (samples <= 0)
        return;

    bus.beginBlock (samples);

    for (auto& slot : slots)
        if (slot->process (samples, midi))
            bus.add (slot->getOutputBuffer(), samples);

    bus.finish (renderBuffer, samples);
}

void AudioEngine::copyToOutput (juce::AudioBuffer<float>& output, int numSamples) noexcept
{
    const int samples = juce::jmin (numSamples,
                                    output.getNumSamples(),
                                    renderBuffer.getNumSamples());
    const int outputChannels = output.getNumChannels();
    const int copyChannels = juce::jmin (numChannels, outputChannels);

    for (int channel = 0; channel < copyChannels; ++channel)
        output.copyFrom (channel, 0, renderBuffer, channel, 0, samples);

    for (int channel = copyChannels; channel < outputChannels; ++channel)
        output.clear (channel, 0, output.getNumSamples());
}

//==============================================================================
void AudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    if (device == nullptr)
        return;

    const double deviceSampleRate = device->getCurrentSampleRate();
    const int deviceBlockSize = device->getCurrentBufferSizeSamples();

    if (deviceSampleRate <= 0.0 || deviceBlockSize <= 0)
        return;

    const bool needsPrepare = ! prepared
                              || std::abs (deviceSampleRate - sampleRate) > 0.5
                              || deviceBlockSize > maximumBlockSize;

    if (needsPrepare)
        prepare (deviceSampleRate,
                 juce::jmax (deviceBlockSize, maximumBlockSize),
                 numChannels);

    // The advertised buffer size can shrink without a re-prepare; the deadline
    // must still follow what the device will actually ask for.
    timingMonitor.prepare (deviceSampleRate, deviceBlockSize);
}

void AudioEngine::audioDeviceStopped()
{
    // Keep slot activation and buffers; the next start fades in again if the
    // device needed a re-prepare.
}

void AudioEngine::audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                                    int numInputChannels,
                                                    float* const* outputChannelData,
                                                    int numOutputChannels,
                                                    int numSamples,
                                                    const juce::AudioIODeviceCallbackContext&)
{
    const RtGuard::Scope rtScope;
    const auto timingToken = timingMonitor.beginCallback();

    if (outputChannelData == nullptr)
    {
        timingMonitor.endCallback (timingToken, 0);
        return;
    }

    if (! prepared)
    {
        for (int channel = 0; channel < numOutputChannels; ++channel)
            if (outputChannelData[channel] != nullptr)
                juce::FloatVectorOperations::clear (outputChannelData[channel], numSamples);

        timingMonitor.endCallback (timingToken, 0);
        return;
    }

    const int samples = juce::jmin (numSamples, maximumBlockSize);

    // A first-class audio-input slot: hand the interface block to any slot
    // source that consumes it, before the graph renders.
    feedDeviceInput (inputChannelData, numInputChannels);

    deviceMidi.clear();
    renderBlock (samples, deviceMidi);

    const int copyChannels = juce::jmin (numChannels, numOutputChannels);

    for (int channel = 0; channel < copyChannels; ++channel)
        if (outputChannelData[channel] != nullptr)
            juce::FloatVectorOperations::copy (outputChannelData[channel],
                                               renderBuffer.getReadPointer (channel),
                                               samples);

    for (int channel = copyChannels; channel < numOutputChannels; ++channel)
        if (outputChannelData[channel] != nullptr)
            juce::FloatVectorOperations::clear (outputChannelData[channel], numSamples);

    if (numSamples > samples)
        for (int channel = 0; channel < copyChannels; ++channel)
            if (outputChannelData[channel] != nullptr)
                juce::FloatVectorOperations::clear (outputChannelData[channel] + samples,
                                                    numSamples - samples);

    timingMonitor.endCallback (timingToken, samples);
}

} // namespace AudioEngine
