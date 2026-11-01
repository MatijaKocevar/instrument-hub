#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace ProtocolTest
{

std::string hexByte (std::uint8_t byte);
std::string hexBytes (const std::vector<std::uint8_t>& bytes);
std::string hexBytes (const std::array<std::uint8_t, 3>& bytes);

}
