#include "LatencyBudget.h"
#include "Fixtures.h"
#include "TestSupport.h"

#include "instruments/AudioInputInstrument.h"

#include <iostream>

namespace AudioInputTest
{

void runLatencyBudget()
{
    std::cout << "\n-- the input's own latency budget --" << std::endl;

    AudioInputInstrument input;
    input.prepare (kSampleRate, kBlockSize);

    check (input.latencyBudgetSamples() == -1, "no latency budget is configured by default");
    check (input.isWithinLatencyBudget(), "an unset budget always passes");

    input.setLatencySamples (200);
    input.setLatencyBudgetSamples (240);

    check (input.latencyBudgetSamples() == 240, "the budget is reported back");
    check (input.isWithinLatencyBudget(), "200 samples fits a 240-sample budget");

    input.setLatencyBudgetSamples (200);
    check (input.isWithinLatencyBudget(), "the budget is inclusive");

    input.setLatencyBudgetSamples (199);
    check (! input.isWithinLatencyBudget(), "200 samples exceeds a 199-sample budget");

    input.setLatencyBudgetSamples (-1);
    check (input.latencyBudgetSamples() == -1 && input.isWithinLatencyBudget(),
           "a negative budget clears the check");
}

}
