#pragma once

#include <string>

namespace ApiTest::Discovery
{

#if ! defined (_WIN32)

bool findAdvertisement (int fd, const std::string& instance, int expectedPort,
                        std::string& detail);

#endif

} // namespace ApiTest::Discovery
