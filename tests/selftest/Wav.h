#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace SelfTest
{

void writeWav (const std::string& path, const std::vector<std::int16_t>& samples);

}
