#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Api::discovery
{

    int createMdnsSocket (const std::string& address, std::string& error);

    void sendPacket (int fd, const std::vector<std::uint8_t>& packet);

}
