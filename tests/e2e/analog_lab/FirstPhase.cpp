#include "Check.h"
#include "analog_lab/Bundle.h"
#include "analog_lab/Constants.h"
#include "analog_lab/FirstPhase.h"
#include "analog_lab/Measurement.h"
#include "analog_lab/SessionRig.h"
#include "analog_lab/StateFile.h"

#include "Session.h"
#include "plugins/PluginInstrument.h"

#include <iostream>
#include <string>

namespace AnalogLab
{
using E2E::check;


int runFirstPhase (const juce::File& stateDirectory, const juce::File& hashFile)
{
    const auto bundle = findAnalogLabBundle();

    if (! bundle.exists())
    {
        std::cout << "SKIP ihub-analog-lab-state: Analog Lab V is not installed"
                  << std::endl;
        return kSkipReturnCode;
    }

    const auto config = makeConfig (bundle.getFullPathName());
    juce::MemoryBlock stateA;
    juce::MemoryBlock stateB;
    std::string hashA;
    std::string hashB;
    Measurement first;

    {
        ihub::Session session;

        if (! buildSession (session, config, stateDirectory))
            return 1;

        if (! waitForAllWarm (session, kSlots))
            return 1;

        if (! armAndWait (session, 0))
            return 1;

        auto* instrumentA = session.getPluginInstrument (0);
        check (instrumentA != nullptr, "patch A has a live Analog Lab V instance");

        if (instrumentA == nullptr)
            return 1;

        check (instrumentA->setParameterValue ("Master", 0.85f),
               "patch A has an Analog Lab Master parameter");
        check (instrumentA->setParameterValue ("P1 VCF Cutoff", 0.25f),
               "patch A has an Analog Lab filter cutoff parameter");
        check (instrumentA->setParameterValue ("P1 Brightness", 0.5f),
               "patch A has an Analog Lab brightness parameter");
        pumpFor (300);
        check (stabiliseState (*instrumentA, stateA),
               "patch A state blob stabilised");
        hashA = hashOf (stateA);
        check (stateA.getSize() > 0,
               "patch A captured a non-empty state ("
                   + std::to_string (stateA.getSize()) + " bytes, hash "
                   + hashA + ")");
        check (session.capturePluginState (0),
               "patch A state was written to the sidecar");

        if (! armAndWait (session, 1))
            return 1;

        auto* instrumentB = session.getPluginInstrument (1);
        check (instrumentB != nullptr, "patch B has a live Analog Lab V instance");

        if (instrumentB == nullptr)
            return 1;

        check (instrumentB->setParameterValue ("Master", 0.35f),
               "patch B has an Analog Lab Master parameter");
        check (instrumentB->setParameterValue ("P1 VCF Cutoff", 0.85f),
               "patch B has an Analog Lab filter cutoff parameter");
        check (instrumentB->setParameterValue ("P1 Brightness", 0.8f),
               "patch B has an Analog Lab brightness parameter");
        pumpFor (300);
        check (stabiliseState (*instrumentB, stateB),
               "patch B state blob stabilised");
        hashB = hashOf (stateB);
        check (stateB.getSize() > 0,
               "patch B captured a non-empty state ("
                   + std::to_string (stateB.getSize()) + " bytes, hash "
                   + hashB + ")");
        check (stateA != stateB && hashA != hashB,
               "the two Analog Lab states are distinct (" + hashA + " vs "
                   + hashB + ")");
        check (session.capturePluginState (1),
               "patch B state was written to the sidecar");
        check (session.savePluginStates(), "the sidecar saved explicitly");

        check (measurePass (session, hashA, hashB, "first pass", first),
               "the first session renders and distinguishes both patches");

        check (first.peakLiveInstances <= 1 + kNeighbourCount,
               "the first process held at most two Analog Lab instances");
    }

    check (E2E::failures == 0, "the first phase completed without failures");

    if (E2E::failures == 0)
        check (writeHashes (hashFile, hashA, hashB),
               "the expected state hashes were written for the restart phase");

    return E2E::failures == 0 ? 0 : 1;
}

}
