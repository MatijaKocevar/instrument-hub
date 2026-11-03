#include "project/Import.h"
#include "project/Reload.h"
#include "project/SessionDescription.h"
#include "project/Support.h"

#include <juce_events/juce_events.h>

#include <cstdlib>
#include <iostream>

#ifndef IHUB_SET_YAML
 #define IHUB_SET_YAML "shared/set.yaml"
#endif

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    setenv ("INSTRUMENT_HUB_SET", IHUB_SET_YAML, 1);

    std::cout << "ihub-project-test: bundled set -> .ihub -> mapping and session\n";

    ProjectTest::runImportChecks();
    ProjectTest::runSessionDescriptionChecks();
    ProjectTest::runReloadChecks();

    std::cout << (ProjectTest::failures == 0 ? "\nPASS" : "\nFAIL")
              << ": " << ProjectTest::failures << " failed check(s)" << std::endl;

    return ProjectTest::failures == 0 ? 0 : 1;
}
