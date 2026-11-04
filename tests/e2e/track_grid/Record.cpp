#include "track_grid/Record.h"

#include "Check.h"
#include "track_grid/Memory.h"

#include <fstream>

namespace TrackGridE2E
{

void writeTrackGridRecord (const TrackGridResult& result,
                           const std::vector<LedEvent>& leds)
{
    std::ofstream record ("ihub-e2e-track-grid-record.txt");

    record << "test=ihub-e2e-track-grid\n"
           << "layout=tracks\n"
           << "tracks=" << result.trackCount << "\n"
           << "protected_slots=" << result.protectedCount << "\n"
           << "warm_instances=" << result.warmCreated << "\n"
           << "live_after_warm=" << result.liveAfterWarm << "\n"
           << "peak_instances=" << result.peakAfterWarm << "\n"
           << "rss_baseline_mb="
           << kilobytesToMegabytes (result.beforeWarm.currentKb) << "\n"
           << "rss_peak_mb=" << kilobytesToMegabytes (result.afterWarm.peakKb) << "\n"
           << "rss_peak_kb=" << result.afterWarm.peakKb << "\n"
           << "switches=" << result.switches << "\n"
           << "warming_events=" << result.warmingSeen << "\n"
           << "instances_after_switches=" << result.creationAfterSwitches << "\n"
           << "evictions_after_switches=" << result.evictionsAfterSwitches << "\n"
           << "fx_actions=" << result.fxToggles << "\n"
           << "volume_actions=" << result.volumeSets << "\n"
           << "red_track_leds=" << result.redTrackLeds << "\n"
           << "led_events=" << result.ledEvents << "\n"
           << "rendered_callbacks=" << result.callbacks << "\n"
           << "xruns=" << result.xruns << "\n"
           << "worst_callback_ms=" << result.worstCallbackMs << "\n"
           << "deadline_ms=" << result.deadlineMs << "\n"
           << "last_switch_rms_dbfs=" << result.lastSwitchRmsDbfs << "\n"
           << "status=" << (E2E::failures == 0 ? "PASS" : "FAIL") << "\n"
           << "\n[led-stream]\n";

    for (const auto& event : leds)
        record << "pad=" << event.pad
               << " velocity=" << static_cast<int> (event.velocity)
               << " mode=" << event.mode << "\n";

    E2E::check (record.good(),
                "the track-grid record was written "
                "(ihub-e2e-track-grid-record.txt)");
}

}
