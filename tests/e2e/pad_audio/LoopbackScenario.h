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

/** Section 4: the same pads on the ALSA Loopback, through a real streaming
    session, with non-silent captured audio. */
LoopbackScenarioResult runLoopbackScenario();

} // namespace PadAudio
