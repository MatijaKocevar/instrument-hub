#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace ihub::sampler
{

/** One decoded and resampled sample, ready for playback at the engine rate.

    `audio` is always at `sampleRate`, which is the engine rate that was
    configured when the file was loaded. The source file's own properties are
    kept for display and for tests.

    Channels are limited to two (mono stays mono, stereo stays stereo, and
    files with more channels contribute their first two). Resampling is done
    offline with a windowed-sinc low-pass, so the buffer is band-limited to the
    engine Nyquist even when the file is at a higher rate.
*/
struct SampleData
{
    juce::AudioBuffer<float> audio;

    /** Sample rate of `audio` (the engine rate). */
    double sampleRate = 0.0;

    /** Sample rate of the file that was decoded. */
    double sourceSampleRate = 0.0;

    /** Frames in the source file. */
    juce::int64 sourceFrames = 0;

    /** Channels in the source file (before the two-channel limit). */
    int sourceChannels = 0;

    juce::String name;
    juce::String path;

    int getNumChannels() const noexcept { return audio.getNumChannels(); }
    int getNumSamples() const noexcept { return audio.getNumSamples(); }
    bool isEmpty() const noexcept { return audio.getNumSamples() == 0 || audio.getNumChannels() == 0; }

    double getLengthSeconds() const noexcept
    {
        return sampleRate > 0.0 ? (double) audio.getNumSamples() / sampleRate : 0.0;
    }

    double getSourceLengthSeconds() const noexcept
    {
        return sourceSampleRate > 0.0 ? (double) sourceFrames / sourceSampleRate : 0.0;
    }
};

} // namespace ihub::sampler
