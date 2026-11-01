#include "CommandLine.h"
#include "TestSupport.h"

#include <iostream>

namespace AppModesTest
{

void runCommandLine()
{
    std::cout << "\n-- command line --" << std::endl;

    checkValue ("--snapshot /tmp/out.png", "--snapshot", "/tmp/out.png");
    checkValue ("--snapshot=/tmp/out.png", "--snapshot", "/tmp/out.png");
    checkValue ("--snapshot \"/tmp/out file.png\"", "--snapshot", "/tmp/out file.png");
    checkValue ("--api-port 18080 --serve", "--api-port", "18080");
    checkValue ("--api-port=18080 --serve", "--api-port", "18080");
    checkValue ("--serve", "--snapshot", "");
    checkValue ("--snapshot", "--snapshot", "");
    checkValue ("--snapshot --serve", "--snapshot", "--serve");
}

} // namespace AppModesTest
