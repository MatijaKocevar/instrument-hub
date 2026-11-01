#pragma once

#include <string>
#include <vector>

namespace LaunchpadMappingTest
{

extern const std::vector<std::string> expectedFamilies;

std::vector<std::string> expectedInstruments (const std::string& family);

}
