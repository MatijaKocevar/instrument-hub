#include "TestSupport.h"
#include "transport/Acceptance.h"
#include "transport/ExternalSync.h"
#include "transport/Position.h"
#include "transport/Projection.h"
#include "transport/Realtime.h"
#include "transport/Snapshot.h"
#include "transport/StartStop.h"
#include "transport/TapTempo.h"
#include "transport/Tempo.h"

#include <iostream>

int main()
{
    AudioTest::Transport::runAcceptance();
    AudioTest::Transport::runStartStop();
    AudioTest::Transport::runPosition();
    AudioTest::Transport::runTempo();
    AudioTest::Transport::runProjection();
    AudioTest::Transport::runTapTempo();
    AudioTest::Transport::runExternalSync();
    AudioTest::Transport::runSnapshot();
    AudioTest::Transport::runRealtime();

    if (AudioTest::failures == 0)
    {
        std::cout << "\nPASS: transport" << std::endl;
        return 0;
    }

    std::cout << "\nFAIL: " << AudioTest::failures << " check(s) failed" << std::endl;
    return 1;
}
