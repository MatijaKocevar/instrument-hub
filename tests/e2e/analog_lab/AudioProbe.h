#pragma once

#include "Session.h"

#include <vector>

namespace AnalogLab
{

struct AudioProbe
{
    double rms = 0.0;
    double peak = 0.0;
    std::vector<float> samples;
};

double toDbfs (double linear);

/** Renders a held note on the armed patch and returns the mono (left) signal. */
AudioProbe renderNote (ihub::Session& session, int slot);

double relativeDifference (const AudioProbe& first, const AudioProbe& second);

} // namespace AnalogLab
