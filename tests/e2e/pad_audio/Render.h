#pragma once

#include "Session.h"

#include <vector>

namespace PadAudio
{

ihub::SessionConfig makeConfig (float fadeMilliseconds);

void renderBlock (ihub::Session& session,
                  juce::AudioBuffer<float>& buffer,
                  std::vector<float>& capture);

struct PadRender
{
    std::vector<float> samples;
    int armedAfterFirst = -1;
    int armedAfterSecond = -1;
    int stateAfterFirst = -1;
    int stateAfterSecond = -1;
    double switchMaxDelta = 0.0;
    int switchSample = 0;
};

PadRender runPadRender (float fadeMilliseconds);

double renderRms (ihub::Session& session, int settleBlocks, int rmsBlocks);

}
