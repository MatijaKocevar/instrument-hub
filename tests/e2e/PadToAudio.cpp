#include "Check.h"
#include "pad_audio/Constants.h"
#include "pad_audio/LoopbackScenario.h"
#include "pad_audio/MappingScenario.h"
#include "pad_audio/PadScenario.h"
#include "pad_audio/PagesScenario.h"
#include "pad_audio/Records.h"
#include "project/ProjectScenario.h"

#include <juce_events/juce_events.h>

#include <iostream>

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    std::cout << "ihub-e2e-test: Launchpad pad -> mapping -> composed session -> ALSA Loopback\n"
              << "  block=" << PadAudio::kBlockSize
              << " sample_rate=" << PadAudio::kSampleRate
              << " slots=" << PadAudio::kDefaultSlots << std::endl;

    PadAudio::runMappingScenario();
    PadAudio::runProjectScenario();

    const auto pad = PadAudio::runPadScenario();
    const auto loopback = PadAudio::runLoopbackScenario();
    const auto pages = PadAudio::runPagesScenario();

    E2E::check (pages.ran,
                "the page acceptance pass ran"
                    + (pages.ran ? std::string() : ": " + pages.error));

    PadAudio::writePagesRecord (pages);

    PadAudio::MeasurementRecord record;
    record.renderRms = pad.renderRms;
    record.renderRmsDbfs = pad.renderRmsDbfs;
    record.switchDiscontinuityDbfs = pad.switchDiscontinuityDbfs;
    record.padSwitchMaxDelta = pad.switchMaxDelta;
    record.loopbackPlayback = loopback.playbackName;
    record.loopbackCapture = loopback.captureName;
    record.loopbackFirstDbfs = loopback.firstDbfs;
    record.loopbackSecondDbfs = loopback.secondDbfs;
    PadAudio::writeMeasurementRecord (record);

    std::cout << (E2E::failures == 0 ? "\nPASS" : "\nFAIL")
              << ": " << E2E::failures << " failed check(s)" << std::endl;

    return E2E::failures == 0 ? 0 : 1;
}
