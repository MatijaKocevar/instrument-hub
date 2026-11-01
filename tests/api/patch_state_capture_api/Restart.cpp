#include "Restart.h"

#include "Fixtures.h"

#include "../TestSupport.h"

#include "plugins/PluginInstrument.h"

#include <iostream>
#include <string>

namespace ApiTest::PatchStateCaptureApi
{

bool runRestart (const juce::File& stateDirectory, const juce::String& bundlePath)
{
    ihub::Session session;
    session.setPluginStateDirectory (stateDirectory);

    check (session.hasPluginState (kPluginSlot)
               && session.getPluginStateSize (kPluginSlot) > 0,
           "the explicitly saved blob is loaded at restart");

    juce::String buildError;

    if (! session.build (makeConfig (bundlePath), buildError))
    {
        std::cerr << "patch-state-capture-api-test: FAILED to rebuild: "
                  << buildError << std::endl;
        return false;
    }

    check (session.waitForSlotWarm (kPluginSlot, 15000),
           "the plugin patch warms after the restart");
    session.armSlot (kPluginSlot);
    check (session.waitForSlotActive (kPluginSlot, 5000),
           "the plugin patch arms after the restart");

    auto* instrument = session.getPluginInstrument (kPluginSlot);
    const auto frequency = instrument != nullptr
                               ? denormaliseFrequency (
                                     instrument->getParameterValue ("Frequency"))
                               : 0.0;
    const auto marker = stateMarker (session, kPluginSlot);
    const auto pitch = measurePitch (session);

    check (approximately (marker, kStateMarkerB, 0.01),
           "the restart restores the saved state marker ("
               + std::to_string (marker) + ")");
    check (approximately (frequency, kFrequencyB, 5.0),
           "the restart restores the saved frequency ("
               + std::to_string (frequency) + " Hz)");
    check (approximately (pitch, kFrequencyB, 40.0),
           "the restart renders the saved sound ("
               + std::to_string (pitch) + " Hz)");

    return true;
}

} // namespace ApiTest::PatchStateCaptureApi
