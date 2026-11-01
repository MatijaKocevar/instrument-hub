#include "Check.h"
#include "pad_audio/Constants.h"
#include "pad_audio/Records.h"

#include "controller/LaunchpadProtocol.h"

#include <fstream>
#include <iostream>

namespace PadAudio
{
using E2E::check;


void writePagesRecord (const PagesResult& pages)
{
    if (! pages.ran)
        return;

    std::ofstream pagesRecord ("ihub-e2e-pages-record.txt");
    pagesRecord << "test=ihub-e2e-pages\n"
                << "pages=grid,instrument,plugins,audio\n"
                << "click_path=POST /arm\n"
                << "click_slot=" << pages.clickSlot << "\n"
                << "click_armed=" << pages.clickArmed << "\n"
                << "click_audio_rms_dbfs=" << pages.clickRmsDbfs << "\n"
                << "pad=" << pages.pad << "\n"
                << "pad_armed=" << pages.padArmed << "\n"
                << "pad_audio_rms_dbfs=" << pages.padRmsDbfs << "\n"
                << "same_patch=" << (pages.samePatch ? "true" : "false") << "\n"
                << "armed_pad=" << kPagePad << "\n"
                << "armed_led_velocity="
                << static_cast<int> (pages.armedLed.velocity) << "\n"
                << "armed_led_mode=" << pages.armedLed.mode << "\n"
                << "armed_led_red="
                << (pages.armedLed.velocity
                        == Launchpad::velocityFor (Launchpad::Colour::Red)
                    ? "true" : "false") << "\n"
                << "idle_pad=" << kPageIdlePad << "\n"
                << "idle_colour_index=" << pages.patchColorIndex << "\n"
                << "idle_led_velocity="
                << static_cast<int> (pages.idleLed.velocity) << "\n"
                << "idle_led_expected="
                << static_cast<int> (pages.idleExpected) << "\n"
                << "idle_led_matches_patch_colour="
                << (pages.idleLed.velocity == pages.idleExpected
                    ? "true" : "false") << "\n"
                << "status=" << (E2E::failures == 0 ? "PASS" : "FAIL") << "\n";

    check (pagesRecord.good(),
           "the page acceptance record was written "
           "(ihub-e2e-pages-record.txt)");
}

void writeMeasurementRecord (const MeasurementRecord& record)
{
    std::cout << "  record: pads=" << kFirstPad << "->" << kSecondPad
              << " render_rms=" << record.renderRms
              << " render_rms_dbfs=" << record.renderRmsDbfs
              << " switch_discontinuity_dbfs=" << record.switchDiscontinuityDbfs
              << " loopback_rms_dbfs_first=" << record.loopbackFirstDbfs
              << " loopback_rms_dbfs_second=" << record.loopbackSecondDbfs
              << std::endl;

    std::ofstream measurement ("ihub-e2e-record.txt");
    measurement << "test=ihub-e2e\n"
                << "pads=" << kFirstPad << "->" << kSecondPad << "\n"
                << "render_rms=" << record.renderRms << "\n"
                << "render_rms_dbfs=" << record.renderRmsDbfs << "\n"
                << "switch_discontinuity_dbfs=" << record.switchDiscontinuityDbfs << "\n"
                << "pad_switch_max_delta=" << record.padSwitchMaxDelta << "\n"
                << "loopback_playback=" << record.loopbackPlayback << "\n"
                << "loopback_capture=" << record.loopbackCapture << "\n"
                << "loopback_rms_dbfs_first=" << record.loopbackFirstDbfs << "\n"
                << "loopback_rms_dbfs_second=" << record.loopbackSecondDbfs << "\n"
                << "status=" << (E2E::failures == 0 ? "PASS" : "FAIL") << "\n";

    check (measurement.good(), "the measurement record was written "
                               "(ihub-e2e-record.txt)");
}

} // namespace PadAudio
