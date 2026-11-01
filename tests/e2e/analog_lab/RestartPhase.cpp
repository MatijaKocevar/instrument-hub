#include "Check.h"
#include "analog_lab/Bundle.h"
#include "analog_lab/Constants.h"
#include "analog_lab/Measurement.h"
#include "analog_lab/RestartPhase.h"
#include "analog_lab/SessionRig.h"

#include "Session.h"

#include <iostream>
#include <string>

namespace AnalogLab
{
using E2E::check;


int runRestartPhase (const juce::File& stateDirectory,
                     const std::string& hashA,
                     const std::string& hashB)
{
    const auto bundle = findAnalogLabBundle();

    if (! bundle.exists())
    {
        std::cout << "SKIP ihub-analog-lab-state: Analog Lab V is not installed"
                  << std::endl;
        return kSkipReturnCode;
    }

    const auto config = makeConfig (bundle.getFullPathName());
    Measurement restarted;

    {
        ihub::Session session;

        if (! buildSession (session, config, stateDirectory))
            return 1;

        if (! waitForAllWarm (session, kSlots))
            return 1;

        check (session.hasPluginState (0) && session.hasPluginState (1),
               "the Analog Lab sidecar is loaded at startup");

        check (measurePass (session, hashA, hashB, "restart", restarted),
               "the restarted session renders and distinguishes both patches");

        check (restarted.peakLiveInstances <= 1 + kNeighbourCount,
               "the restarted process held at most two Analog Lab instances");
    }

    check (E2E::failures == 0, "the restart phase completed without failures");
    return E2E::failures == 0 ? 0 : 1;
}

} // namespace AnalogLab
