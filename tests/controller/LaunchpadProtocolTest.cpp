#include "TestSupport.h"
#include "protocol/Buttons.h"
#include "protocol/Frames.h"
#include "protocol/Grid.h"
#include "protocol/Leds.h"

#include <iostream>

int main()
{
    ProtocolTest::testGrid();
    ProtocolTest::testFrames();
    ProtocolTest::testLeds();
    ProtocolTest::testButtons();

    if (ControllerTest::failures != 0)
    {
        std::cout << ControllerTest::failures << " check(s) failed" << std::endl;
        return 1;
    }

    std::cout << "PASS: Launchpad Mini MK3 protocol" << std::endl;
    return 0;
}
