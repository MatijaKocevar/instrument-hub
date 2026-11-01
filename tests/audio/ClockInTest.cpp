#include "TestSupport.h"
#include "clock_in/Acceptance.h"
#include "clock_in/ClockLoss.h"
#include "clock_in/InternalSync.h"
#include "clock_in/Jitter.h"
#include "clock_in/Midi.h"
#include "clock_in/Queue.h"
#include "clock_in/Realtime.h"
#include "clock_in/Reset.h"
#include "clock_in/TempoChange.h"
#include "clock_in/Tempos.h"
#include "clock_in/Transport.h"

#include <iostream>

int main()
{
    AudioTest::ClockIn::runAcceptance();
    AudioTest::ClockIn::runTempos();
    AudioTest::ClockIn::runTempoChange();
    AudioTest::ClockIn::runJitter();
    AudioTest::ClockIn::runTransport();
    AudioTest::ClockIn::runClockLoss();
    AudioTest::ClockIn::runQueue();
    AudioTest::ClockIn::runInternalSync();
    AudioTest::ClockIn::runMidi();
    AudioTest::ClockIn::runRealtime();
    AudioTest::ClockIn::runReset();

    if (AudioTest::failures == 0)
    {
        std::cout << "\nPASS: clock in" << std::endl;
        return 0;
    }

    std::cout << "\nFAIL: " << AudioTest::failures << " check(s) failed" << std::endl;
    return 1;
}
