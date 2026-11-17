#pragma once

#include "api/state/AudioState.h"

#include <vector>

namespace AudioEngine::detail {

int chooseProbeCandidate(const std::vector<Api::AudioProbeState>& candidates,
                         double preferredSampleRate);

void finalizeAudioWizard(Api::AudioWizardState& wizard, double preferredSampleRate);

}  // namespace AudioEngine::detail
