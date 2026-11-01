#include "Check.h"
#include "analog_lab/AudioProbe.h"
#include "analog_lab/Constants.h"
#include "analog_lab/Measurement.h"
#include "analog_lab/SessionRig.h"
#include "analog_lab/StateFile.h"

#include <iostream>
#include <string>

namespace AnalogLab
{
using E2E::check;


bool measurePass (ihub::Session& session,
                  const std::string& hashA,
                  const std::string& hashB,
                  const std::string& phase,
                  Measurement& measurement)
{
    if (! armAndWait (session, 0))
        return false;

    check (verifySlotState (session, 0, hashA, phase + " A"),
           phase + ": patch A keeps its state hash");

    measurement.audioA = renderNote (session, 0);

    if (! armAndWait (session, 1))
        return false;

    check (verifySlotState (session, 1, hashB, phase + " B"),
           phase + ": patch B keeps its state hash");

    measurement.audioB = renderNote (session, 1);

    if (! armAndWait (session, 0))
        return false;

    check (verifySlotState (session, 0, hashA, phase + " A return"),
           phase + ": patch A keeps its state hash after A -> B -> A");

    measurement.difference = relativeDifference (measurement.audioA,
                                                 measurement.audioB);
    measurement.peakLiveInstances = session.getPeakInstrumentCount();

    std::cout << phase << " AUDIO A: rms=" << measurement.audioA.rms << " ("
              << toDbfs (measurement.audioA.rms) << " dBFS), peak="
              << measurement.audioA.peak << std::endl;
    std::cout << phase << " AUDIO B: rms=" << measurement.audioB.rms << " ("
              << toDbfs (measurement.audioB.rms) << " dBFS), peak="
              << measurement.audioB.peak << std::endl;
    std::cout << phase << " AUDIO relative difference: "
              << measurement.difference << std::endl;

    bool ok = true;

    auto expect = [&ok, &phase] (bool condition, const std::string& what)
    {
        check (condition, phase + ": " + what);
        ok = ok && condition;
    };

    expect (measurement.audioA.rms > kSilenceFloor,
            "patch A renders a non-silent note");
    expect (measurement.audioB.rms > kSilenceFloor,
            "patch B renders a non-silent note");
    expect (measurement.audioA.peak > 1.0e-3 && measurement.audioB.peak > 1.0e-3,
            "both patches have a usable peak level");
    expect (measurement.difference > kMinimumRelativeDifference,
            "the two patch renders differ (relative difference > "
                + std::to_string (kMinimumRelativeDifference) + ")");
    expect (measurement.peakLiveInstances <= 1 + kNeighbourCount,
            "at most two Analog Lab instances were alive (peak "
                + std::to_string (measurement.peakLiveInstances) + ")");

    return ok;
}

}
