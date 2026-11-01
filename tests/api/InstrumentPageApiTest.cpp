#include "TestSupport.h"
#include "instrument_page_api/Inserts.h"
#include "instrument_page_api/Mixer.h"
#include "instrument_page_api/PluginSource.h"
#include "instrument_page_api/Source.h"

#include <juce_events/juce_events.h>

#include <iostream>

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    auto config = ApiTest::makeToneConfig (ApiTest::InstrumentPageApi::kSlots);
    config.neighbourCount = 1;

    ApiTest::SessionRig rig (config, 20, "instrument-page-test");

    if (! rig.started())
        return 1;

    std::cout << "instrument-page-test: listening on 127.0.0.1:" << rig.port()
              << " slots=" << rig.session().getNumSlots() << std::endl;

    ApiTest::InstrumentPageApi::runMixer (rig);
    ApiTest::InstrumentPageApi::runInserts (rig);
    ApiTest::InstrumentPageApi::runSource (rig);
    ApiTest::InstrumentPageApi::runPluginSource (rig);

    rig.stop();

    if (ApiTest::failures > 0)
    {
        std::cerr << "instrument-page-test: " << ApiTest::failures
                  << " check(s) failed" << std::endl;
        return 1;
    }

    std::cout << "instrument-page-test: all checks passed" << std::endl;
    return 0;
}
