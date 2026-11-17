#pragma once

#include "audio/LatencyProbe.h"

#include <vector>

namespace AudioEngine::detail {

struct CapturedLoopbackImpulse {
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

CapturedLoopbackImpulse captureLoopbackImpulse(const LatencyProbeOptions& options);

}  // namespace AudioEngine::detail
