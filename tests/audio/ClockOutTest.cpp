#include "TestSupport.h"
#include "clock_out/Acceptance.h"
#include "clock_out/Midi.h"
#include "clock_out/Overflow.h"
#include "clock_out/Realtime.h"
#include "clock_out/Rewind.h"
#include "clock_out/Tempo.h"
#include "clock_out/Transport.h"

#include <iostream>

int main()
{
    AudioTest::ClockOut::runAcceptance();
    AudioTest::ClockOut::runTransport();
    AudioTest::ClockOut::runTempo();
    AudioTest::ClockOut::runRewind();
    AudioTest::ClockOut::runMidi();
    AudioTest::ClockOut::runRealtime();
    AudioTest::ClockOut::runOverflow();

    if (AudioTest::failures == 0)
    {
        std::cout << "\nPASS: clock out" << std::endl;
        return 0;
    }

    std::cout << "\nFAIL: " << AudioTest::failures << " check(s) failed" << std::endl;
    return 1;
}
