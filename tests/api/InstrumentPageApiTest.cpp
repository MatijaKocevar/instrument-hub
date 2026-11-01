// ihub-instrument-page-test: acceptance check for T124.
//
// The instrument page's controls all end in the control API, so this test
// drives the same requests the page sends and measures what the engine does:
//
//   - the volume fader (PATCH volumeDb) changes the RENDERED level, not just
//     the JSON: -20 dB drops the armed slot's RMS to a tenth, 0 dB restores it;
//   - mute silences the slot and unmute restores it, and pan reaches /state;
//   - the insert chain can be built, bypassed, reordered, replaced and cleared
//     through the same endpoints the page's chain editor uses;
//   - the instrument source can be swapped: a new tone frequency is audible
//     (measured by zero crossings) and a VST3 source loads, becomes active and
//     renders non-silent audio (when TestPlugin is built);
//   - every invalid request is rejected without half-applying the change.
//
// Everything is offline and deterministic: the test drives the render calls,
// so the result does not depend on a sound card or wall-clock timing.

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
