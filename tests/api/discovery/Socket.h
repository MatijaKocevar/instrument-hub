#pragma once

#include <string>

namespace ApiTest::Discovery
{

#if ! defined (_WIN32)

std::string localAddress();

int openQuerySocket();

void closeQuerySocket (int fd);

#endif

}
