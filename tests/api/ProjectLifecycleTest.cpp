#include "project/ProjectLifecycle.h"
#include "project/TrackSetup.h"

#include "TestSupport.h"

#include <juce_events/juce_events.h>

#include <iostream>

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    std::cout << "ihub-project-lifecycle-api-test: project new/open/save-as/close\n";

    const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("ihub-project-lifecycle-"
                                         + juce::Uuid().toString());
    root.createDirectory();

    ApiTest::ProjectLifecycle::runLifecycle (root);
    ApiTest::ProjectLifecycle::runHistory (root);
    ApiTest::ProjectLifecycle::runTrackSetup (root);

    std::cout << (ApiTest::failures == 0 ? "\nPASS" : "\nFAIL")
              << ": " << ApiTest::failures << " failed check(s)" << std::endl;

    root.deleteRecursively();
    return ApiTest::failures == 0 ? 0 : 1;
}
