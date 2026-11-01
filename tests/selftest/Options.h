#pragma once

#include <string>

namespace SelfTest
{

struct Options
{
    std::string playback;
    std::string capture;
    std::string output;
};

void printUsage (const char* argv0);
bool parseOptions (int argc, char** argv, Options& options);

} // namespace SelfTest
