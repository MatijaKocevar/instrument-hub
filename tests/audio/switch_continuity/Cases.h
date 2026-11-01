#pragma once

#include <string>

namespace AudioTest::SwitchContinuity
{

void runCase (const std::string& label, float fadeMilliseconds, bool expectDetections);

}
