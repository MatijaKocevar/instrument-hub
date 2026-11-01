#include "Check.h"
#include "pad_audio/Constants.h"
#include "pad_audio/Metrics.h"
#include "pad_audio/PadScenario.h"
#include "pad_audio/Render.h"

#include "Session.h"

#include <iostream>
#include <string>

namespace PadAudio
{
using E2E::check;


PadScenarioResult runPadScenario()
{
    PadScenarioResult result;

    const auto normal = runPadRender (3.0f);

    if (normal.samples.empty())
    {
        check (false, "the pad-driven offline render produced samples");
    }
    else
    {
        check (normal.armedAfterFirst == kFirstPad,
               "pad " + std::to_string (kFirstPad) + " arms slot "
                   + std::to_string (kFirstPad) + " (got "
                   + std::to_string (normal.armedAfterFirst) + ")");
        check (normal.armedAfterSecond == kSecondPad,
               "pad " + std::to_string (kSecondPad) + " arms slot "
                   + std::to_string (kSecondPad) + " (got "
                   + std::to_string (normal.armedAfterSecond) + ")");
        check (normal.stateAfterFirst
                   == static_cast<int> (AudioEngine::SlotSwitcher::State::active),
               "the first pad's slot is active after the fade");
        check (normal.stateAfterSecond
                   == static_cast<int> (AudioEngine::SlotSwitcher::State::active),
               "the second pad's slot is active after the fade");

        const int firstRmsEnd = (kSettleBlocks + kRmsBlocks) * kBlockSize;
        const auto firstRms = rmsDbfs (normal.samples, kSettleBlocks * kBlockSize, firstRmsEnd);
        const auto secondRms = rmsDbfs (normal.samples,
                                        firstRmsEnd + 4 * kBlockSize,
                                        static_cast<int> (normal.samples.size()));

        std::cout << "  pad render: first_rms_dbfs=" << firstRms
                  << " second_rms_dbfs=" << secondRms
                  << " switch_max_delta=" << normal.switchMaxDelta << std::endl;

        check (firstRms > kMinimumRmsDbfs, "the first armed slot is non-silent");
        check (secondRms > kMinimumRmsDbfs, "the second armed slot is non-silent");

        const auto firstTone = toneMagnitude (normal.samples, 0, firstRmsEnd, 110.0);
        const auto firstOther = toneMagnitude (normal.samples, 0, firstRmsEnd, 146.83);
        const auto secondTone = toneMagnitude (normal.samples,
                                               firstRmsEnd + 4 * kBlockSize,
                                               static_cast<int> (normal.samples.size()),
                                               146.83);
        const auto secondOther = toneMagnitude (normal.samples,
                                                firstRmsEnd + 4 * kBlockSize,
                                                static_cast<int> (normal.samples.size()),
                                                110.0);

        std::cout << "  tone identity: slot0=110Hz (" << firstTone << " vs "
                  << firstOther << ") slot3=146.83Hz (" << secondTone << " vs "
                  << secondOther << ")" << std::endl;

        check (firstTone > 3.0 * firstOther,
               "the first window is slot 0's 110 Hz tone");
        check (secondTone > 3.0 * secondOther,
               "the second window is slot 3's 146.83 Hz tone");

        check (normal.switchMaxDelta <= kMaximumSwitchDelta,
               "the pad-triggered switch has no sample step above "
                   + std::to_string (kMaximumSwitchDelta));
    }

    {
        ihub::Session session;
        juce::String error;

        if (session.build (makeConfig (3.0f), error))
        {
            const auto report = session.measureSwitch (kFirstPad, kSecondPad);

            result.renderRms = report.rms;
            result.renderRmsDbfs = report.rmsDbfs;
            result.switchDiscontinuityDbfs = report.discontinuityDbfs;

            std::cout << "  switch measurement: rms=" << report.rms
                      << " rms_dbfs=" << report.rmsDbfs
                      << " discontinuity_dbfs=" << report.discontinuityDbfs
                      << " max_slew_dbfs=" << report.maxSampleDeltaDbfs
                      << " samples=" << report.samplesMeasured << std::endl;

            check (report.rendered, "the switch measurement rendered samples");
            check (report.rmsDbfs > kMinimumRmsDbfs,
                   "the switch measurement reports a non-silent RMS");
            check (report.discontinuityDbfs <= kDiscontinuityLimitDbfs,
                   "the switch is click-free (residual <= -60 dBFS)");
        }
        else
        {
            check (false, "switch-measurement session builds: " + error.toStdString());
        }
    }

    {
        const auto hard = runPadRender (0.0f);
        result.switchMaxDelta = normal.switchMaxDelta;

        std::cout << "  hard-switch control: switch_max_delta="
                  << hard.switchMaxDelta << std::endl;

        check (! hard.samples.empty(), "the hard-switch control rendered samples");
        check (hard.switchMaxDelta > kHardSwitchMinimumDelta,
               "the pad-switch delta detector fires on a hard switch");
    }

    return result;
}

}
