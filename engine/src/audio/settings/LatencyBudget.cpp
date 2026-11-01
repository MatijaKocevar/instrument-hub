#include "audio/settings/LatencyBudget.h"

namespace AudioEngine::detail
{

Api::AudioLatencyState computeLatencyBudget (int bufferSize,
                                             double sampleRate,
                                             int deviceLatencySamples,
                                             int chainSamples)
{
    Api::AudioLatencyState budget;

    budget.bufferMs = static_cast<double> (bufferSize) * 1000.0 / sampleRate;
    budget.deviceMs = static_cast<double> (deviceLatencySamples) * 1000.0 / sampleRate;
    budget.chainMs = static_cast<double> (chainSamples) * 1000.0 / sampleRate;
    budget.totalMs = budget.bufferMs + budget.deviceMs + budget.chainMs;
    return budget;
}

} // namespace AudioEngine::detail
