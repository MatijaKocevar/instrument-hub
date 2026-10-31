#include "AudioInputInstrument.h"

#include <cmath>

namespace ihub
{

void AudioInputInstrument::prepare (double newSampleRate, int maxBlock)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    maximumBlockSize = juce::jmax (1, maxBlock);
}

void AudioInputInstrument::setInputChannel (int channel) noexcept
{
    selectedInputChannel.store (juce::jmax (0, channel), std::memory_order_relaxed);
}

int AudioInputInstrument::getInputChannel() const noexcept
{
    return selectedInputChannel.load (std::memory_order_relaxed);
}

void AudioInputInstrument::setGain (float linearGain) noexcept
{
    gainTarget.store (juce::jlimit (0.0f, maximumGain, linearGain),
                      std::memory_order_relaxed);
}

float AudioInputInstrument::getGain() const noexcept
{
    return gainTarget.load (std::memory_order_relaxed);
}

void AudioInputInstrument::setLatencySamples (int samples) noexcept
{
    cachedLatency.store (juce::jmax (0, samples), std::memory_order_relaxed);
}

int AudioInputInstrument::latencySamples() const noexcept
{
    return cachedLatency.load (std::memory_order_relaxed);
}

//==============================================================================
void AudioInputInstrument::setDirectMonitor (bool shouldMonitor) noexcept
{
    directMonitor.store (shouldMonitor, std::memory_order_relaxed);
}

bool AudioInputInstrument::isDirectMonitor() const noexcept
{
    return directMonitor.load (std::memory_order_relaxed);
}

void AudioInputInstrument::setLatencyBudgetSamples (int samples) noexcept
{
    latencyBudget.store (samples < 0 ? -1 : samples, std::memory_order_relaxed);
}

int AudioInputInstrument::latencyBudgetSamples() const noexcept
{
    return latencyBudget.load (std::memory_order_relaxed);
}

bool AudioInputInstrument::isWithinLatencyBudget() const noexcept
{
    const int budget = latencyBudget.load (std::memory_order_relaxed);

    if (budget < 0)
        return true;

    return cachedLatency.load (std::memory_order_relaxed) <= budget;
}

bool AudioInputInstrument::bypassesFxChain() const noexcept
{
    return isDirectMonitor();
}

void AudioInputInstrument::setInputBlock (const float* const* channelData,
                                          int numChannels) noexcept
{
    inputChannels = channelData;
    numInputChannels = juce::jmax (0, numChannels);
}

void AudioInputInstrument::clearInput() noexcept
{
    inputChannels = nullptr;
    numInputChannels = 0;
}

void AudioInputInstrument::process (juce::AudioBuffer<float>& buffer,
                                    juce::MidiBuffer&)
{
    const int channel = selectedInputChannel.load (std::memory_order_relaxed);

    const float* input = nullptr;

    if (inputChannels != nullptr && channel >= 0 && channel < numInputChannels)
        input = inputChannels[channel];

    // One block only: never keep a pointer to a device buffer across callbacks.
    clearInput();

    const int samples = juce::jmin (buffer.getNumSamples(), maximumBlockSize);
    const int totalSamples = buffer.getNumSamples();

    if (input == nullptr || samples <= 0)
    {
        buffer.clear();
        return;
    }

    const float gain = juce::jlimit (0.0f, maximumGain,
                                     gainTarget.load (std::memory_order_relaxed));
    const int channels = buffer.getNumChannels();

    if (gain <= 0.0f)
    {
        buffer.clear();
        return;
    }

    // Unity is the default and must stay bit-exact: skip the multiply so the
    // master bus sees the input samples unchanged.
    if (std::abs (gain - 1.0f) <= 0.0f)
    {
        for (int outChannel = 0; outChannel < channels; ++outChannel)
            buffer.copyFrom (outChannel, 0, input, samples);
    }
    else
    {
        for (int outChannel = 0; outChannel < channels; ++outChannel)
        {
            auto* destination = buffer.getWritePointer (outChannel);

            for (int i = 0; i < samples; ++i)
                destination[i] = input[i] * gain;
        }
    }

    // Never leave stale samples in a block larger than the prepared maximum.
    if (totalSamples > samples)
        for (int outChannel = 0; outChannel < channels; ++outChannel)
            buffer.clear (outChannel, samples, totalSamples - samples);
}

void AudioInputInstrument::noteOn (int, int, float)
{
}

void AudioInputInstrument::noteOff (int, int)
{
}

} // namespace ihub
