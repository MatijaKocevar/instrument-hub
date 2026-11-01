#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "MasterLimiter.h"

namespace AudioEngine
{


/**
    The summing bus: every processing slot is added into one pre-allocated mix
    buffer, then the master limiter runs once over the result.

    Buffers are allocated only in `prepare()`; `beginBlock()`, `add()` and
    `finish()` are audio-thread calls and never allocate or lock.
*/
class Bus
{
public:
    Bus() = default;

    /** Allocates the mix buffer and prepares the limiter. Off the audio thread. */
    void prepare (double sampleRate, int maximumBlockSize, int numChannels);

    /** Releases the buffers. The audio stream must be stopped. */
    void releaseResources();

    /** Clears the mix without releasing memory. Audio thread. */
    void reset() noexcept;

    /** Clears the region of the mix that the next block will use. */
    void beginBlock (int numSamples) noexcept;

    /** Adds one slot's output (or any buffer with the same layout). */
    void add (const juce::AudioBuffer<float>& source, int numSamples) noexcept;

    /** Applies the master limiter and copies the result to `destination`. */
    void finish (juce::AudioBuffer<float>& destination, int numSamples) noexcept;

    int getNumChannels() const noexcept { return numChannels; }
    int getMaximumBlockSize() const noexcept { return maximumBlockSize; }

    MasterLimiter& getLimiter() noexcept { return limiter; }
    const MasterLimiter& getLimiter() const noexcept { return limiter; }

    const juce::AudioBuffer<float>& getMixBuffer() const noexcept { return mixBuffer; }

private:
    juce::AudioBuffer<float> mixBuffer;
    MasterLimiter limiter;
    double sampleRate = 48000.0;
    int maximumBlockSize = 0;
    int numChannels = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Bus)
};

} // namespace AudioEngine
