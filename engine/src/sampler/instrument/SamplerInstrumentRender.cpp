#include "../SamplerInstrument.h"

#include "SamplerInstrumentConstants.h"

#include <cmath>

namespace ihub::sampler
{

using namespace instrument_detail;

//==============================================================================
// Voice rendering and block scheduling

void SamplerInstrument::renderVoices (juce::AudioBuffer<float>& buffer,
                                      int startSample, int numSamples) noexcept
{
    const int channels = buffer.getNumChannels();
    const int totalSamples = buffer.getNumSamples();

    if (channels <= 0 || numSamples <= 0)
        return;

    const int first = juce::jlimit (0, totalSamples, startSample);
    const int last = juce::jlimit (first, totalSamples, startSample + numSamples);

    if (last <= first)
        return;

    auto* left = buffer.getWritePointer (0);
    auto* right = channels > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int sample = first; sample < last; ++sample)
    {
        float leftSum = 0.0f;
        float rightSum = 0.0f;

        for (auto& voice : voices)
        {
            if (! voice.active)
                continue;

            const auto& source = *voice.sample;
            const int frames = source.audio.getNumSamples();

            if (frames <= 0 || voice.position >= (double) frames)
            {
                setVoiceInactive (voice);
                continue;
            }

            const int index = (int) std::floor (voice.position);
            const int next = juce::jmin (index + 1, frames - 1);
            const auto fraction = (float) (voice.position - (double) index);
            const int sourceChannels = source.audio.getNumChannels();

            const auto* sourceLeft = source.audio.getReadPointer (0);
            const auto* sourceRight = sourceChannels > 1 ? source.audio.getReadPointer (1)
                                                         : sourceLeft;

            const float leftSample = sourceLeft[index]
                                     + fraction * (sourceLeft[next] - sourceLeft[index]);
            const float rightSample = sourceRight[index]
                                      + fraction * (sourceRight[next] - sourceRight[index]);

            const float envelope = voice.gain;
            leftSum += leftSample * voice.leftGain * envelope;
            rightSum += rightSample * voice.rightGain * envelope;

            if (voice.fadeStep > 0.0f)
            {
                voice.gain -= voice.fadeStep;

                if (voice.gain <= 0.0f)
                {
                    voice.gain = 0.0f;
                    setVoiceInactive (voice);
                }
            }

            voice.position += voice.ratio;
        }

        if (right != nullptr)
        {
            left[sample] += leftSum;
            right[sample] += rightSum;
        }
        else
        {
            left[sample] += (leftSum + rightSum) * monoPanSum;
        }
    }
}

void SamplerInstrument::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    buffer.clear();

    const int numSamples = buffer.getNumSamples();

    if (numSamples <= 0)
        return;

    // Events queued by noteOn()/noteOff() belong at the start of this block.
    drainQueuedEvents();

    int rendered = 0;

    auto event = midi.cbegin();
    const auto end = midi.cend();

    while (rendered < numSamples)
    {
        int nextEventPosition = numSamples;

        if (event != end)
        {
            const auto position = (*event).samplePosition;

            if (position > rendered)
                nextEventPosition = juce::jmin (numSamples, position);
        }

        if (nextEventPosition > rendered)
        {
            renderVoices (buffer, rendered, nextEventPosition - rendered);
            rendered = nextEventPosition;
        }

        // An event timestamped at or past the end of the block belongs to a
        // later block; do not let it sound early.
        if (rendered >= numSamples)
            break;

        bool handled = false;

        while (event != end && (*event).samplePosition <= rendered)
        {
            applyMidiEvent (*event);
            ++event;
            handled = true;
        }

        if (! handled)
        {
            // Defensive: an event with a negative position or an iterator that
            // did not advance must not stall the block. Render one sample and
            // continue; events are still checked again on the next iteration.
            renderVoices (buffer, rendered, 1);
            ++rendered;
        }
    }
}

} // namespace ihub::sampler
