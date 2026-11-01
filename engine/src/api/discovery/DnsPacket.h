#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace Api
{

struct DiscoveryService;

/** Builds the complete mDNS response packet for `service`. */
std::vector<std::uint8_t> buildResponse (const DiscoveryService& service, bool goodbye);

/** True when a received datagram is a query for one of our records. */
bool wantsResponse (const std::uint8_t* data, std::size_t size,
                    const DiscoveryService& service);

} // namespace Api
