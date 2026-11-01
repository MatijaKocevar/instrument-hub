#include "TestSupport.h"
#include "CrossCorrelation.h"
#include "Loopback.h"

#include <juce_events/juce_events.h>

#include <iostream>

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    LatencyProbeTest::runCrossCorrelation();
    LatencyProbeTest::runLoopbackLatency();

    std::cout << (LatencyProbeTest::failures == 0 ? "PASS" : "FAIL")
              << ": ihub-latency-probe (" << LatencyProbeTest::failures
              << " failure(s))" << std::endl;

    return LatencyProbeTest::failures == 0 ? 0 : 1;
}
