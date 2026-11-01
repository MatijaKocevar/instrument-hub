#pragma once

#include "api/Server.h"

namespace AudioEngine::detail
{

/** The latency budget published by `GET /audio`.

    `deviceLatencySamples` is the open device's input + output latency in
    samples, or 0 when no device is open.
*/
Api::AudioLatencyState computeLatencyBudget (int bufferSize,
                                             double sampleRate,
                                             int deviceLatencySamples,
                                             int chainSamples);

} // namespace AudioEngine::detail
