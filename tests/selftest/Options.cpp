#include "Options.h"
#include "Constants.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace SelfTest
{

void printUsage (const char* argv0)
{
    std::cout << "usage: " << argv0 << " [--out FILE] [--playback DEVICE] [--capture DEVICE]\n"
              << "  defaults: --out out.wav --playback " << kDefaultPlayback
              << " --capture " << kDefaultCapture << std::endl;
}

bool parseOptions (int argc, char** argv, Options& options)
{
    options.playback = kDefaultPlayback;
    options.capture = kDefaultCapture;
    options.output = kDefaultOutput;

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];

        if (arg == "--help" || arg == "-h")
        {
            printUsage (argv[0]);
            return false;
        }

        if (arg == "--out" && i + 1 < argc)
            options.output = argv[++i];
        else if (arg == "--playback" && i + 1 < argc)
            options.playback = argv[++i];
        else if (arg == "--capture" && i + 1 < argc)
            options.capture = argv[++i];
        else
            throw std::runtime_error ("unknown or incomplete argument: " + arg);
    }

    return true;
}

} // namespace SelfTest
