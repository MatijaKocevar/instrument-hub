#include "TestSupport.h"
#include "mapping/Document.h"
#include "mapping/Fixtures.h"
#include "mapping/HotReload.h"
#include "mapping/Tracks.h"
#include "mapping/Validation.h"

#include <juce_events/juce_events.h>

#include <iostream>
#include <string>

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    std::cout << "ihub-mapping-test\n"
              << "  block: " << MappingTest::kBlockSize
              << ", sample rate: " << MappingTest::kSampleRate
              << ", hot-reload budget: " << MappingTest::kHotReloadBudgetMs
              << " ms" << std::endl;

    MappingTest::runDocumentChecks();
    MappingTest::runValidationChecks();
    MappingTest::runTrackChecks();
    MappingTest::runHotReloadChecks();

    std::cout << (ControllerTest::failures == 0 ? "\nPASS" : "\nFAIL")
              << ": " << ControllerTest::failures << " failed check(s)" << std::endl;

    return ControllerTest::failures == 0 ? 0 : 1;
}
