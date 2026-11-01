#include "audio/LatencyProbe.h"

#include "audio/latency/LatencyCapture.h"

namespace AudioEngine
{

LatencyProbe::LatencyProbe() = default;
LatencyProbe::~LatencyProbe() = default;

LatencyResult LatencyProbe::measure (int bufferSize, double sampleRate)
{
    LatencyProbeOptions options;
    options.bufferSize = bufferSize;
    options.sampleRate = sampleRate;
    return measure (options);
}

LatencyResult LatencyProbe::measure (const LatencyProbeOptions& options)
{
    LatencyResult last;

    for (int attempt = 0; attempt < 3; ++attempt)
    {
        last = measureOnce (options);

        if (last.valid)
            return last;
    }

    return last;
}

LatencyResult LatencyProbe::measureOnce (const LatencyProbeOptions& options)
{
    LatencyResult result;
    result.sampleRate = options.sampleRate;
    result.bufferSize = options.bufferSize;

    const auto capture = detail::captureLoopbackImpulse (options);

    result.outputDeviceName = capture.playbackDevice;
    result.inputDeviceName = capture.captureDevice;

    if (capture.sampleRate > 0.0)
        result.sampleRate = capture.sampleRate;

    if (capture.bufferSize > 0)
        result.bufferSize = capture.bufferSize;

    if (! capture.valid)
    {
        result.error = capture.error;
        return result;
    }

    const int captureSamples = static_cast<int> (capture.reference.size());
    const int maxLag = juce::jmin (captureSamples - capture.impulseOffset - 1,
                                   captureSamples - 1);

    if (maxLag < 0)
    {
        result.error = "capture window is shorter than the impulse offset";
        return result;
    }

    float confidence = 0.0f;
    const int lag = findLagSamples (capture.reference.data(),
                                    captureSamples,
                                    capture.captured.data(),
                                    captureSamples,
                                    maxLag,
                                    &confidence);

    if (lag < 0 || confidence <= 0.0f)
    {
        result.error = "no impulse was found in the capture";
        return result;
    }

    result.valid = true;
    result.roundTripSamples = lag;
    result.roundTripMs = static_cast<double> (lag) * 1000.0 / result.sampleRate;
    result.confidence = confidence;
    return result;
}

}
