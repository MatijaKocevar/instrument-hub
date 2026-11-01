#include "TestSupport.h"
#include "CommandLine.h"
#include "Modes.h"

#include <iostream>

int main()
{
    AppModesTest::runCommandLine();
    AppModesTest::runModes();

    if (AppModesTest::failures > 0)
    {
        std::cerr << "app-modes: " << AppModesTest::failures
                  << " check(s) failed" << std::endl;
        return 1;
    }

    std::cout << "app-modes: all checks passed" << std::endl;
    return 0;
}
