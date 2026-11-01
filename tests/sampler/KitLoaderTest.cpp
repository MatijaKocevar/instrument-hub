#include "Fixtures.h"
#include "TestSupport.h"
#include "kit/AudioFixtures.h"
#include "kit/Layers.h"
#include "kit/Load.h"
#include "kit/Parse.h"
#include "kit/Resolve.h"
#include "kit/Validation.h"

#include <juce_events/juce_events.h>

#include <iostream>

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const auto directory = SamplerTest::testDirectory ("kit-fixtures");

    std::cout << "ihub-kit-loader-test\n"
              << "fixtures: " << directory.getFullPathName() << "\n"
              << std::endl;

    KitTest::writeFixtures (directory);
    KitTest::runParseChecks();

    ihub::sampler::LoadedKit kit;
    KitTest::runLayersChecks (directory, kit);
    KitTest::runLoadChecks (directory);
    KitTest::runValidationChecks (directory, kit);
    KitTest::runResolveChecks (kit);

    std::cout << std::endl;

    if (SamplerTest::failures == 0)
    {
        std::cout << "PASS: kit format and velocity layers (T091)" << std::endl;
        return 0;
    }

    std::cout << "FAIL: " << SamplerTest::failures << " check(s) failed" << std::endl;
    return 1;
}
