// ihub-session-smoke-test: acceptance check for T114.
//
// Composes the default session (AudioEngine + SlotSwitcher + eight instrument
// slots), arms slot 0, measures its RMS, switches to slot 1 and measures the
// discontinuity across the 3 ms equal-power crossfade.
//
// Acceptance:
//   - the armed slot produces a non-zero RMS (well above -60 dBFS),
//   - no sample discontinuity above -60 dBFS across the switch,
//   - the same detector catches a hard switch with the ramp disabled, so the
//     check can fail,
//   - the session's render path is free of allocation/lock/logging under
//     RtGuard.
//
// Everything is offline and deterministic: no audio device is opened and the
// result does not depend on wall-clock timing.

#include "Session.h"
#include "audio/RtGuard.h"

#include <juce_events/juce_events.h>

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 128;
constexpr int kNumSlots = 8;
constexpr double kDiscontinuityLimitDbfs = -60.0;
constexpr double kMinimumRmsDbfs = -60.0;

int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

ihub::SessionConfig makeConfig (float fadeMilliseconds)
{
    ihub::SessionConfig config;
    config.device.sampleRate = kSampleRate;
    config.device.bufferSize = kBlockSize;
    config.device.numOutputChannels = 2;
    config.device.numInputChannels = 2;
    config.fadeMilliseconds = fadeMilliseconds;
    config.slots = ihub::Session::makeDefaultSet();
    return config;
}

void checkReport (const ihub::SessionSwitchReport& report, const std::string& label)
{
    std::cout << "      " << label
              << ": rms=" << report.rms
              << " (" << report.rmsDbfs << " dBFS)"
              << " discontinuity=" << report.discontinuityDbfs << " dBFS"
              << " max_delta=" << report.maxSampleDeltaDbfs << " dBFS"
              << " samples=" << report.samplesMeasured << std::endl;

    check (report.rendered, label + ": the render produced samples");
    check (report.samplesMeasured > 0, label + ": samples were measured");
    check (report.rms > 0.0 && report.rmsDbfs > kMinimumRmsDbfs,
           label + ": the armed slot has a non-zero RMS");
    check (report.discontinuityDbfs <= kDiscontinuityLimitDbfs,
           label + ": no discontinuity above -60 dBFS across the switch");
}

/** Renders one callback through the device-facing path and returns its RMS. */
double renderCallbackRms (ihub::Session& session)
{
    const int blockSize = session.getEngine().getMaximumBlockSize();
    std::vector<float> left (static_cast<std::size_t> (blockSize), 0.0f);
    std::vector<float> right (static_cast<std::size_t> (blockSize), 0.0f);
    float* outputs[2] { left.data(), right.data() };

    const juce::AudioIODeviceCallbackContext context;
    session.audioDeviceIOCallbackWithContext (nullptr, 0, outputs, 2, blockSize, context);

    double sumSquares = 0.0;

    for (int frame = 0; frame < blockSize; ++frame)
        sumSquares += static_cast<double> (left[static_cast<std::size_t> (frame)])
                      * static_cast<double> (left[static_cast<std::size_t> (frame)]);

    return blockSize > 0 ? std::sqrt (sumSquares / static_cast<double> (blockSize)) : 0.0;
}

} // namespace

//==============================================================================
int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    std::cout << "ihub-session-smoke-test\n"
              << "  slots: " << kNumSlots
              << ", block: " << kBlockSize
              << ", sample rate: " << kSampleRate << std::endl;

    //==========================================================================
    // 1. The default set composes into eight fixed slots.
    {
        ihub::Session session;
        juce::String error;

        const bool built = session.build (makeConfig (3.0f), error);

        check (built, "the default session builds" + (built ? std::string() : ": " + error.toStdString()));
        check (session.isPrepared(), "the session is prepared");
        check (session.getNumSlots() == kNumSlots, "eight slots were created");
        check (static_cast<int> (session.getSlotConfigs().size()) == kNumSlots,
               "the default set describes eight slots");
        check (session.getSlotConfigs()[0].kind == ihub::SessionSlotKind::initTone,
               "the default slots are initialisation tones");
        check (session.getEngine().getBus().getNumChannels() == 2,
               "the summing bus is stereo");
        check (session.getEngine().getBus().getMaximumBlockSize() == kBlockSize,
               "the bus is sized at the maximum block size");
    }

    //==========================================================================
    // 2. The armed slot is audible and a switch is click-free.
    {
        ihub::Session session;
        juce::String error;

        if (! session.build (makeConfig (3.0f), error))
        {
            check (false, "the default session builds: " + error.toStdString());
        }
        else
        {
            AudioEngine::RtGuard::resetCounters();

            const auto forward = session.measureSwitch (0, 1);
            const auto reverse = session.measureSwitch (1, 0);

            const auto counters = AudioEngine::RtGuard::getCounters();

            checkReport (forward, "slot 0 -> slot 1");
            checkReport (reverse, "slot 1 -> slot 0");

            check (counters.total() == 0,
                   "the session render path allocates/locks/logs nothing ("
                       + std::to_string (counters.allocations) + " alloc, "
                       + std::to_string (counters.locks) + " lock, "
                       + std::to_string (counters.logs) + " log)");
            check (AudioEngine::RtGuard::assertNoOpenScopes(),
                   "no realtime scope leaked out of the render");

            // The device-facing callback shares the render path; drive one
            // block through it and require the armed slot to be non-silent.
            session.armSlot (1);
            const auto callbackRms = renderCallbackRms (session);
            check (callbackRms > 0.0,
                   "the device callback path produces non-zero audio");
        }
    }

    //==========================================================================
    // 3. Negative control: with the ramp disabled the detector must fire.
    {
        ihub::Session session;
        juce::String error;

        if (! session.build (makeConfig (0.0f), error))
        {
            check (false, "the hard-switch session builds: " + error.toStdString());
        }
        else
        {
            const auto hard = session.measureSwitch (0, 1);

            std::cout << "      hard switch: discontinuity=" << hard.discontinuityDbfs
                      << " dBFS max_delta=" << hard.maxSampleDeltaDbfs
                      << " dBFS" << std::endl;

            check (hard.rendered, "the hard-switch render produced samples");
            check (hard.discontinuityDbfs > kDiscontinuityLimitDbfs,
                   "the discontinuity detector fires on a hard switch");
        }
    }

    std::cout << (failures == 0 ? "\nPASS" : "\nFAIL")
              << ": " << failures << " failed check(s)" << std::endl;

    return failures == 0 ? 0 : 1;
}
