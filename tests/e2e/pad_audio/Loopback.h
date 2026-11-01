#pragma once

#include "Session.h"
#include "pad_audio/Rig.h"

#include <string>
#include <vector>

namespace PadAudio
{

struct LoopbackResult
{
    std::vector<float> captured;
    int armedAfterFirst = -1;
    int armedAfterSecond = -1;
    juce::String playbackName;
    juce::String captureName;
    juce::String error;
    bool ran = false;
};

LoopbackResult runLoopback (ihub::Session& session, PadRig& rig);

}
