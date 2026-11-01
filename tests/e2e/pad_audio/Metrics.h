#pragma once

#include <vector>

namespace PadAudio
{

double toDbfs (double linear) noexcept;

double rmsDbfs (const std::vector<float>& samples, int first, int last);

double maxDelta (const std::vector<float>& samples, int first, int last);

/** Magnitude of one frequency in a window (a single-bin DFT). */
double toneMagnitude (const std::vector<float>& samples,
                      int first,
                      int last,
                      double frequency);

} // namespace PadAudio
