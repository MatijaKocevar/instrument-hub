#include "Cases.h"

#include "Constants.h"
#include "DeltaDetector.h"
#include "Render.h"
#include "SpectralDetector.h"

#include "../TestSupport.h"

#include <iostream>

namespace AudioTest::SwitchContinuity
{

using AudioTest::check;

void runCase (const std::string& label, float fadeMilliseconds, bool expectDetections)
{
    std::cout << "\n-- " << label << " (fade " << fadeMilliseconds << " ms) --" << std::endl;

    const auto rendered = renderRun (fadeMilliseconds);

    check (rendered.switchesIssued == kSwitchCount,
           "50 switches were issued");
    check (static_cast<int> (rendered.switchSamples.size()) == kSwitchCount,
           "50 switches reached the mixer");
    check (rendered.switchesPerSlot[0] > 0
               && rendered.switchesPerSlot[1] > 0
               && rendered.switchesPerSlot[2] > 0
               && rendered.switchesPerSlot[3] > 0,
           "all four slots were selected at least once");
    check (rendered.limiterStayedUnity,
           "the master limiter stayed at unity (linear path, no gain riding)");
    check (rendered.peak < 0.5,
           "peak stays well below the limiter threshold");
    check (static_cast<int> (rendered.master.size())
               >= kPreRollSamples + kTailSamples,
           "the master bus was captured for the whole run");

    const auto delta = analyseDelta (rendered.master);
    const auto spectral = analyseSpectrum (rendered.master);

    std::cout << "      delta threshold: " << kDeltaThreshold
              << ", measured max |x[n]-x[n-1]|: " << delta.maxDelta
              << " at sample " << delta.worstSample << std::endl;
    std::cout << "      delta detections: " << delta.detections << std::endl;
    std::cout << "      spectral threshold: " << kSpectralThresholdDb
              << " dB, measured max splatter: " << spectral.maxSplatterDb
              << " dB at frame " << spectral.worstFrame
              << " (" << spectral.framesAnalysed << " frames)" << std::endl;
    std::cout << "      spectral detections: " << spectral.detections << std::endl;
    std::cout << "      frames above -70/-60/-50/-40/-30/-20 dB:";
    for (auto count : spectral.countsAbove)
        std::cout << ' ' << count;
    std::cout << std::endl;

    if (expectDetections)
    {
        check (delta.detections > 0,
               "the sample-to-sample delta detector fires when the ramp is disabled");
        check (spectral.detections > 0,
               "the spectrum-splatter detector fires when the ramp is disabled");
    }
    else
    {
        check (delta.maxDelta <= kDeltaThreshold,
               "no sample-to-sample delta above the threshold");
        check (delta.detections == 0,
               "delta detector: zero detections across 50 switches");
        check (spectral.maxSplatterDb <= kSpectralThresholdDb,
               "no frame exceeds the splatter threshold");
        check (spectral.detections == 0,
               "spectrum detector: zero detections across 50 switches");
    }
}

}
