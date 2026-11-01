#include "Hex.h"

#include <iomanip>
#include <sstream>

namespace ProtocolTest
{

std::string hexByte (std::uint8_t byte)
{
    std::ostringstream stream;
    stream << "0x" << std::uppercase << std::hex << std::setw (2)
           << std::setfill ('0') << static_cast<int> (byte);
    return stream.str();
}

std::string hexBytes (const std::vector<std::uint8_t>& bytes)
{
    std::string result;

    for (const auto byte : bytes)
        result += (result.empty() ? "" : " ") + hexByte (byte);

    return result;
}

std::string hexBytes (const std::array<std::uint8_t, 3>& bytes)
{
    return hexBytes (std::vector<std::uint8_t> (bytes.begin(), bytes.end()));
}

}
