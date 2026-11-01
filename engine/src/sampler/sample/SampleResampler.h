#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace ihub::sampler::sample_detail
{

/** The engine sample rates the decoder accepts. */
bool isSupportedEngineRate (double rate) noexcept;

/** Polyphase windowed-sinc resampler.

    Output sample n is the source waveform evaluated at
    `n * sourceRate / destinationRate`; the output length is rounded to the
    nearest frame so the duration matches the source. Samples outside the
    source are treated as silence, which is what a one-shot sampler wants.
*/
juce::AudioBuffer<float> resampleBuffer (const juce::AudioBuffer<float>& source,
                                         double sourceRate,
                                         double destinationRate);

} // namespace ihub::sampler::sample_detail
