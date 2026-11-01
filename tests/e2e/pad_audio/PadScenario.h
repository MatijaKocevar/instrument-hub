#pragma once

namespace PadAudio
{

struct PadScenarioResult
{
    double switchMaxDelta = 0.0;
    double renderRms = 0.0;
    double renderRmsDbfs = -1000.0;
    double switchDiscontinuityDbfs = -1000.0;
};

PadScenarioResult runPadScenario();

}
