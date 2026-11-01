#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace AudioEngine
{

/**
    A zero-latency peak limiter for the master bus.

    Below the threshold it is bit-exact unity. Above it, gain is reduced
    instantaneously to the value that maps the peak onto the threshold, then
    released exponentially. There is no lookahead, so it cannot overshoot on
    attack: a sample is always multiplied by a gain that is at most
    `threshold / peak`.

    `process()` is realtime-safe: no allocation, no locks.
*/
class MasterLimiter
{
public:
    void prepare (double sampleRate,
                  float thresholdDb = -0.3f,
                  float releaseMilliseconds = 50.0f) noexcept;

    void reset() noexcept;

    /** Limits `numSamples` of `buffer` in place. */
    void process (juce::AudioBuffer<float>& buffer, int numSamples) noexcept;

    void setThresholdDb (float thresholdDb) noexcept;
    float getThresholdDb() const noexcept { return thresholdInDb; }

    /** Current gain reduction as a negative dB value (0 dB when inactive). */
    float getGainReductionDb() const noexcept;

    float getCurrentGain() const noexcept { return gain; }

private:
    float threshold = 0.9660509f;
    float thresholdInDb = -0.3f;
    float gain = 1.0f;
    float releaseCoefficient = 0.0f;
};

} // namespace AudioEngine
