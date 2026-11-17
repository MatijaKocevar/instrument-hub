#include "PanGains.h"

#include <juce_core/juce_core.h>

#include <cmath>

namespace ihub::sampler {

float panGainLeft(float pan, float gain) noexcept {
    const auto clamped = juce::jlimit(-1.0f, 1.0f, pan);
    const auto angle = (clamped + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
    return std::cos(angle) * gain;
}

float panGainRight(float pan, float gain) noexcept {
    const auto clamped = juce::jlimit(-1.0f, 1.0f, pan);
    const auto angle = (clamped + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
    return std::sin(angle) * gain;
}

}  // namespace ihub::sampler
