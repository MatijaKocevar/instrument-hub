#include "Check.h"
#include "pad_audio/Constants.h"
#include "pad_audio/Loopback.h"
#include "pad_audio/LoopbackScenario.h"
#include "pad_audio/Metrics.h"
#include "pad_audio/Render.h"
#include "pad_audio/Rig.h"

#include "Session.h"

#include <iostream>

#ifndef IHUB_SET_YAML
 #define IHUB_SET_YAML "shared/set.yaml"
#endif

namespace PadAudio
{
using E2E::check;


LoopbackScenarioResult runLoopbackScenario()
{
    LoopbackScenarioResult result;

    ihub::Session session;
    juce::String error;

    if (! session.build (makeConfig (3.0f), error))
    {
        check (false, "loopback session builds: " + error.toStdString());
        return result;
    }

    PadRig rig (session);
    std::string mappingError;

    if (! rig.load (IHUB_SET_YAML, mappingError))
    {
        check (false, "loopback set.yaml loads: " + mappingError);
        return result;
    }

    const auto loopback = runLoopback (session, rig);

    if (! loopback.ran)
    {
        check (false, "the ALSA Loopback pass runs: "
                          + loopback.error.toStdString());
        return result;
    }

    const int firstWindow = 16 * kBlockSize;
    const int secondWindow = 96 * kBlockSize;
    const int end = static_cast<int> (loopback.captured.size());

    const auto capturedFirst = rmsDbfs (loopback.captured, firstWindow,
                                        firstWindow + 16 * kBlockSize);
    const auto capturedSecond = rmsDbfs (loopback.captured, secondWindow, end);
    const auto capturedDelta = maxDelta (loopback.captured,
                                         kLoopbackWarmupBlocks * kBlockSize,
                                         end);

    result.firstDbfs = capturedFirst;
    result.secondDbfs = capturedSecond;
    result.playbackName = loopback.playbackName.toStdString();
    result.captureName = loopback.captureName.toStdString();

    std::cout << "  loopback: playback=\"" << loopback.playbackName << "\""
              << " capture=\"" << loopback.captureName << "\""
              << " captured_first_rms_dbfs=" << capturedFirst
              << " captured_second_rms_dbfs=" << capturedSecond
              << " captured_max_delta=" << capturedDelta
              << " master_peak=" << session.getMasterMeterPeak()
              << std::endl;

    check (loopback.armedAfterFirst == kFirstPad,
           "the loopback pad 0 press arms slot 0");
    check (loopback.armedAfterSecond == kSecondPad,
           "the loopback pad 3 press arms slot 3");
    check (capturedFirst > kMinimumRmsDbfs,
           "the loopback capture is non-silent for the first pad");
    check (capturedSecond > kMinimumRmsDbfs,
           "the loopback capture is non-silent for the second pad");
    check (session.getMasterMeterPeak() > 0.0f,
           "the session master meter reports the rendered bus");

    std::cout << "  loopback switch delta (informational): "
              << capturedDelta << std::endl;

    return result;
}

}
