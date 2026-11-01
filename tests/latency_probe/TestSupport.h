#pragma once

#include <string>

namespace LatencyProbeTest
{

extern int failures;

void check (bool condition, const std::string& what);

} // namespace LatencyProbeTest
