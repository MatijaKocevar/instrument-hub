#include "TestSupport.h"
#include "launchpad_mapping/Document.h"
#include "launchpad_mapping/Mapping.h"
#include "launchpad_mapping/Validation.h"

#include <iostream>

int main()
{
    std::cout << "ihub-launchpad-mapping-test\n";

    if (! LaunchpadMappingTest::runDocumentChecks())
        return 1;

    LaunchpadMappingTest::runMappingChecks();
    LaunchpadMappingTest::runValidationChecks();

    std::cout << (ControllerTest::failures == 0 ? "\nPASS" : "\nFAIL") << ": "
              << ControllerTest::failures << " failed check(s)" << std::endl;

    return ControllerTest::failures == 0 ? 0 : 1;
}
