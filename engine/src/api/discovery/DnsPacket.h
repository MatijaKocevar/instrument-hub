#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace Api
{

struct DiscoveryService;

std::vector<std::uint8_t> buildResponse (const DiscoveryService& service, bool goodbye);

bool wantsResponse (const std::uint8_t* data, std::size_t size,
                    const DiscoveryService& service);

}
