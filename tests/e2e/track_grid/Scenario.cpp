#include "track_grid/Scenario.h"

#include "Check.h"
#include "track_grid/Checks.h"
#include "track_grid/Constants.h"
#include "track_grid/Memory.h"
#include "track_grid/Record.h"
#include "track_grid/Result.h"
#include "track_grid/Rig.h"

#include "Session.h"
#include "plugins/SandboxHost.h"

#include <iostream>
#include <string>

namespace TrackGridE2E
{

void runTrackGridScenario()
{
    TrackGridResult result;
    Rig rig;
    std::string error;

    if (! rig.build (error))
    {
        E2E::check (false, "the track-grid rig builds: " + error);
        return;
    }

    auto& session = rig.session();
    result.trackCount = session.getNumSlots();
    result.protectedCount = session.getWarmProtectedCount();
    result.warmCreated = session.getWarmCreationCount();
    result.liveAfterWarm = session.getLiveInstrumentCount();
    result.peakAfterWarm = session.getPeakInstrumentCount();
    result.beforeWarm = rig.beforeWarm();
    result.afterWarm = rig.afterWarm();

    E2E::check (result.trackCount == kTrackCount,
                "the new project builds eight track slots");
    E2E::check (session.getLayout() == ihub::SessionLayout::tracks,
                "the new project reports the track layout");
    E2E::check (result.protectedCount == kTrackCount,
                "all eight tracks are protected warm slots");
    E2E::check (result.warmCreated == kTrackCount,
                "the warm pool created exactly eight instances");
    E2E::check (result.liveAfterWarm == kTrackCount,
                "all eight TestPlugin instances are live");
    E2E::check (result.peakAfterWarm <= kTrackCount,
                "the live count never exceeds the protected set");
    E2E::check (session.getWarmEvictionCount() == 0,
                "warming the eight tracks evicts nothing");

    bool everyInstrument = true;
    bool everyProcess = true;

    for (int slot = 0; slot < kTrackCount; ++slot)
    {
        const auto warm = session.getWarmState (slot);
        bool instrument = false;

        if (auto* sandbox = session.getSandboxHost (slot); sandbox != nullptr)
            instrument = sandbox->isRunning();
        else
            instrument = session.getPluginInstrument (slot) != nullptr;

        everyInstrument = everyInstrument
                          && warm != ihub::SessionWarmState::cold
                          && instrument;
        everyProcess = everyProcess && session.getSandboxProcessId (slot) > 0;
    }

    E2E::check (everyInstrument,
                "every track holds a warm TestPlugin instrument");
    E2E::check (everyProcess,
                "every track hosts TestPlugin in its sandbox process");

    const double baselineMb = kilobytesToMegabytes (result.beforeWarm.currentKb);
    const double peakMb = kilobytesToMegabytes (result.afterWarm.peakKb);
    const double deltaMb = peakMb - baselineMb;

    std::cout << "  track-grid memory: baseline_rss_mb=" << baselineMb
              << " peak_rss_mb=" << peakMb
              << " delta_mb=" << deltaMb
              << " peak_rss_kb=" << result.afterWarm.peakKb << std::endl;

    session.getEngine().getTimingMonitor().reset();

    runPadSwitchPass (rig, result);
    runApiSwitchPass (rig, result);
    runTrackActions (rig, result);
    checkTrackLeds (rig, result);

    const auto timing = session.getEngine().getTimingMonitor().snapshot();

    result.callbacks = timing.callbacks;
    result.xruns = timing.overruns;
    result.worstCallbackMs = timing.worstCallbackMs;
    result.deadlineMs = timing.deadlineMs;

    std::cout << "  track-grid switches: count=" << result.switches
              << " warming=" << result.warmingSeen
              << " instances=" << result.creationAfterSwitches
              << " evictions=" << result.evictionsAfterSwitches
              << " fx=" << result.fxToggles
              << " volume=" << result.volumeSets << std::endl;
    std::cout << "  track-grid timing: callbacks=" << result.callbacks
              << " xruns=" << result.xruns
              << " worst_ms=" << result.worstCallbackMs
              << " deadline_ms=" << result.deadlineMs << std::endl;

    E2E::check (result.switches == 2 * kTrackCount,
                "both passes switch through all eight tracks");
    E2E::check (result.warmingSeen == 0,
                "no track switch ever reports warming");
    E2E::check (result.creationAfterSwitches == kTrackCount,
                "switching all eight tracks creates no new instances");
    E2E::check (result.evictionsAfterSwitches == 0,
                "switching all eight tracks evicts no instances");
    E2E::check (result.fxToggles >= 3,
                "the pad and API hot-FX paths both applied");
    E2E::check (result.volumeSets >= 4,
                "the pad and API volume paths both applied");
    E2E::check (session.getWarmEvictionCount() == 0,
                "the full performance pass still has zero evictions");
    E2E::check (result.callbacks > 0,
                "the performance pass rendered measured audio blocks");
    E2E::check (result.xruns == 0,
                "the performance pass has no xruns ("
                    + std::to_string (result.xruns) + " overruns)");
    E2E::check (result.worstCallbackMs < result.deadlineMs,
                "every rendered block finished inside the audio deadline");

    result.ran = true;
    writeTrackGridRecord (result, rig.ledEvents());
}

}
