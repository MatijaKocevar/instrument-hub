#pragma once

#include "../TestSupport.h"

#include "audio/Transport.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>

namespace AudioTest::Transport
{

using AudioTest::check;
using AudioTest::nearlyEqual;

constexpr double acceptanceTolerancePpq =
    1.0 / AudioEngine::Transport::ppqTicksPerQuarterNote;

void advance (AudioEngine::Transport& transport, std::int64_t frames, int blockSize);

} // namespace AudioTest::Transport
