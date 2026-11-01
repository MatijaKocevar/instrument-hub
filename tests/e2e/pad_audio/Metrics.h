#pragma once

#include <vector>

namespace PadAudio
{

double toDbfs (double linear) noexcept;

double rmsDbfs (const std::vector<float>& samples, int first, int last);

double maxDelta (const std::vector<float>& samples, int first, int last);

double toneMagnitude (const std::vector<float>& samples,
                      int first,
                      int last,
                      double frequency);

}
