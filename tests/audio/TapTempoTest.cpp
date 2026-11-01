#include "TestSupport.h"
#include "tap_tempo/Acceptance.h"
#include "tap_tempo/Median.h"
#include "tap_tempo/Range.h"
#include "tap_tempo/Realtime.h"
#include "tap_tempo/Reset.h"
#include "tap_tempo/SequenceReset.h"
#include "tap_tempo/TransportIntegration.h"

#include <iostream>

int main()
{
    std::cout << "ihub-tap-tempo-test" << std::endl;

    AudioTest::TapTempo::runAcceptance();
    AudioTest::TapTempo::runMedian();
    AudioTest::TapTempo::runSequenceReset();
    AudioTest::TapTempo::runRange();
    AudioTest::TapTempo::runReset();
    AudioTest::TapTempo::runRealtime();
    AudioTest::TapTempo::runTransportIntegration();

    if (AudioTest::failures == 0)
    {
        std::cout << "\nPASS: tap tempo" << std::endl;
        return 0;
    }

    std::cout << "\nFAIL: " << AudioTest::failures << " check(s) failed" << std::endl;
    return 1;
}
