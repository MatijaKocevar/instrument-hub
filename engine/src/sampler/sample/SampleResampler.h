#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace ihub::sampler::sample_detail {

bool isSupportedEngineRate(double rate) noexcept;

juce::AudioBuffer<float> resampleBuffer(const juce::AudioBuffer<float>& source, double sourceRate,
                                        double destinationRate);

}  // namespace ihub::sampler::sample_detail
