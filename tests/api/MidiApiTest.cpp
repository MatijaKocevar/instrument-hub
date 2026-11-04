#include "TestSupport.h"
#include "midi_api/DeviceEnumeration.h"
#include "midi_api/GenericGrid.h"
#include "midi_api/PadTest.h"
#include "midi_api/Persistence.h"
#include "midi_api/Routes.h"

#include <iostream>

int main()
{
    ApiTest::MidiApi::runRoutes();
    ApiTest::MidiApi::runDeviceEnumeration();
    ApiTest::MidiApi::runPadTestStream();
    ApiTest::MidiApi::runPersistence();
    ApiTest::MidiApi::runGenericGridRouting();
    ApiTest::MidiApi::runGridLearnPersistence();

    if (ApiTest::failures > 0)
    {
        std::cerr << "midi-api-test: " << ApiTest::failures
                  << " check(s) failed" << std::endl;
        return 1;
    }

    std::cout << "midi-api-test: all checks passed" << std::endl;
    return 0;
}
