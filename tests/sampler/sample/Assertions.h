#pragma once

#include "sampler/SampleLoader.h"

#include <string>

namespace SampleTest
{

void checkPitch (const ihub::sampler::SampleData& sample, double expected,
                 const std::string& label, double toleranceHz = 5.0);

} // namespace SampleTest
