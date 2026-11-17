#pragma once

#include "api/Server.h"

namespace AudioEngine::detail {

Api::AudioLatencyState computeLatencyBudget(int bufferSize, double sampleRate,
                                            int deviceLatencySamples,
                                            int chainSamples);

}
