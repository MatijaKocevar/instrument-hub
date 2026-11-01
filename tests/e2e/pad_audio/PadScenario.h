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

/** Sections 2-3: a pad press arms and renders, the switch is click-free,
    and the detector fires when the ramp is disabled. */
PadScenarioResult runPadScenario();

} // namespace PadAudio
