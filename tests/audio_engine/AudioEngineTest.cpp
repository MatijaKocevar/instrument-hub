#include "Fixtures.h"
#include "Limiter.h"
#include "Mix.h"
#include "Realtime.h"
#include "Scheduling.h"

#include <iostream>

int main()
{
    std::cout << "ihub-audio-engine-test: T011 fixed slot graph" << std::endl;

    AudioEngineTest::runMixAgainstReference();
    AudioEngineTest::runOnlyActiveAndFadingSlotsProcess();
    AudioEngineTest::runNoAllocationsDuringProcessBlock();
    AudioEngineTest::runMasterLimiter();

    std::cout << (AudioEngineTest::failures == 0 ? "\nPASS" : "\nFAIL")
              << ": ihub-audio-engine (" << AudioEngineTest::failures
              << " failure(s))" << std::endl;

    return AudioEngineTest::failures == 0 ? 0 : 1;
}
