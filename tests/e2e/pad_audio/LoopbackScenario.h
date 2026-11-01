#pragma once

#include <string>

namespace PadAudio
{

struct LoopbackScenarioResult
{
    double firstDbfs = -1000.0;
    double secondDbfs = -1000.0;
    std::string playbackName;
    std::string captureName;
};

LoopbackScenarioResult runLoopbackScenario();

}
