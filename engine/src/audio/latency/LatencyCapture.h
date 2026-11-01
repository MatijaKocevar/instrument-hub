#pragma once

#include "audio/LatencyProbe.h"

#include <vector>

namespace AudioEngine::detail
{

/** One impulse sent through a loopback and the signals it produced.

    `reference` is the emitted impulse and `captured` is the recorded input,
    both already converted to float and the same length. `sampleRate` and
    `bufferSize` carry the values the device actually accepted.
*/
struct CapturedLoopbackImpulse
{
    bool valid = false;

    juce::String playbackDevice;
    juce::String captureDevice;

    double sampleRate = 0.0;
    int bufferSize = 0;

    int impulseOffset = 0;
    std::vector<float> reference;
    std::vector<float> captured;

    juce::String error;
};

/** Streams one impulse through the ALSA Loopback and records the result. */
CapturedLoopbackImpulse captureLoopbackImpulse (const LatencyProbeOptions& options);

} // namespace AudioEngine::detail
