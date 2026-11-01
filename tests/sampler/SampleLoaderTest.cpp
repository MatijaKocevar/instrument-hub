#include "TestSupport.h"
#include "Fixtures.h"
#include "sample/Decode.h"
#include "sample/Failure.h"
#include "sample/Handoff.h"
#include "sample/Resample.h"
#include "sample/Stress.h"

#include <juce_events/juce_events.h>

#include <iostream>

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const auto directory = SamplerTest::testDirectory ("sample-loader-fixtures");

    std::cout << "ihub-sample-loader-test\n"
              << "fixtures: " << directory.getFullPathName() << "\n"
              << std::endl;

    SampleTest::runDecodeChecks (directory);
    SampleTest::runResampleChecks (directory);
    SampleTest::runHandoffChecks (directory);
    SampleTest::runFailureChecks (directory);
    SampleTest::runStressChecks();

    std::cout << std::endl;

    if (SamplerTest::failures == 0)
    {
        std::cout << "PASS: sample loader (T090)" << std::endl;
        return 0;
    }

    std::cout << "FAIL: " << SamplerTest::failures << " check(s) failed" << std::endl;
    return 1;
}
