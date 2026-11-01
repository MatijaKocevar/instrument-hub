#include "TestSupport.h"
#include "compressor/Bypass.h"
#include "compressor/Dynamics.h"
#include "compressor/Latency.h"
#include "compressor/MakeupGain.h"
#include "compressor/Realtime.h"
#include "compressor/State.h"
#include "compressor/StaticCurve.h"
#include "compressor/StereoLink.h"

#include <juce_events/juce_events.h>

#include <iostream>

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    DspTest::Compressor::runStaticCurve();
    DspTest::Compressor::runMakeupGain();
    DspTest::Compressor::runLatency();
    DspTest::Compressor::runBypass();
    DspTest::Compressor::runStereoLink();
    DspTest::Compressor::runDynamics();
    DspTest::Compressor::runRealtime();
    DspTest::Compressor::runState();

    std::cout << (DspTest::failures == 0 ? "\nPASS" : "\nFAIL") << ": "
              << DspTest::failures << " failed check(s)" << std::endl;

    return DspTest::failures == 0 ? 0 : 1;
}
