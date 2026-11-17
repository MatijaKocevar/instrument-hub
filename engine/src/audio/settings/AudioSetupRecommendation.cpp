#include "audio/settings/AudioSetupRecommendation.h"

#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

namespace AudioEngine::detail {

namespace {
bool isStable(const Api::AudioProbeState& probe) noexcept {
    return probe.opened && probe.xruns == 0;
}

bool betterStable(const Api::AudioProbeState& candidate, const Api::AudioProbeState& best,
                  double preferredSampleRate) {
    if (candidate.bufferSize != best.bufferSize) return candidate.bufferSize < best.bufferSize;

    if (std::abs(candidate.latencyMs - best.latencyMs) > 0.001)
        return candidate.latencyMs < best.latencyMs;

    return std::abs(candidate.sampleRate - preferredSampleRate) <
           std::abs(best.sampleRate - preferredSampleRate);
}

bool betterFallback(const Api::AudioProbeState& candidate, const Api::AudioProbeState& best) {
    if (candidate.xruns != best.xruns) return candidate.xruns < best.xruns;

    if (candidate.bufferSize != best.bufferSize) return candidate.bufferSize < best.bufferSize;

    return candidate.latencyMs < best.latencyMs;
}

std::string describeLatency(double latencyMs) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2) << latencyMs;
    return stream.str();
}

std::string buildTradeoff(const std::vector<Api::AudioProbeState>& candidates, int chosen) {
    const auto& best = candidates[static_cast<std::size_t>(chosen)];
    int unstable = 0;
    int lowestUnstableBuffer = 0;
    std::uint64_t unstableXruns = 0;

    for (const auto& probe : candidates) {
        if (probe.opened && probe.xruns > 0) {
            ++unstable;

            if (lowestUnstableBuffer == 0 || probe.bufferSize < lowestUnstableBuffer) {
                lowestUnstableBuffer = probe.bufferSize;
                unstableXruns = probe.xruns;
            }
        }
    }

    std::string text =
        "Lower buffers reduce latency but leave less time for the "
        "audio callback.";

    if (unstable > 0) {
        text += " " + std::to_string(lowestUnstableBuffer) + " frames produced " +
                std::to_string(unstableXruns) + " xrun(s); " + std::to_string(best.bufferSize) +
                " frames was the lowest buffer " + "with none.";
    } else {
        text += " Every tested buffer was stable, so the lowest buffer wins.";
    }

    text += " The recommended round trip is " + describeLatency(best.latencyMs) + " ms.";
    return text;
}
}  // namespace

int chooseProbeCandidate(const std::vector<Api::AudioProbeState>& candidates,
                         double preferredSampleRate) {
    int best = -1;
    bool bestStable = false;

    for (int index = 0; index < static_cast<int>(candidates.size()); ++index) {
        const auto& probe = candidates[static_cast<std::size_t>(index)];

        if (!probe.opened) continue;

        const bool stable = isStable(probe);

        if (best < 0) {
            best = index;
            bestStable = stable;
            continue;
        }

        const auto& current = candidates[static_cast<std::size_t>(best)];

        if (stable != bestStable) {
            if (stable) {
                best = index;
                bestStable = true;
            }

            continue;
        }

        if (stable ? betterStable(probe, current, preferredSampleRate)
                   : betterFallback(probe, current)) {
            best = index;
        }
    }

    return best;
}

void finalizeAudioWizard(Api::AudioWizardState& wizard, double preferredSampleRate) {
    const int chosen = chooseProbeCandidate(wizard.candidates, preferredSampleRate);

    if (chosen < 0) {
        wizard.valid = false;
        wizard.error = "none of the tested audio settings could be opened";

        for (const auto& probe : wizard.candidates)
            if (!probe.error.empty()) {
                wizard.error += ": " + probe.error;
                break;
            }

        return;
    }

    for (auto& probe : wizard.candidates) probe.recommended = false;

    auto& best = wizard.candidates[static_cast<std::size_t>(chosen)];
    best.recommended = true;

    wizard.valid = true;
    wizard.error.clear();
    wizard.sampleRate = best.sampleRate;
    wizard.bufferSize = best.bufferSize;
    wizard.latencyMs = best.latencyMs;
    wizard.measuredMs = best.measuredMs;
    wizard.expectedMs = best.expectedMs;
    wizard.xruns = best.xruns;
    wizard.tradeoff = buildTradeoff(wizard.candidates, chosen);
}

}  // namespace AudioEngine::detail
