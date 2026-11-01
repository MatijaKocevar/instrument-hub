#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Api::discovery
{

/** Creates and joins the shared mDNS socket, or returns -1 with `error` set. */
int createMdnsSocket (const std::string& address, std::string& error);

/** Sends one mDNS packet to the multicast group. */
void sendPacket (int fd, const std::vector<std::uint8_t>& packet);

} // namespace Api::discovery
