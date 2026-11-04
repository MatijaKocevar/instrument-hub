#include "track_grid/Checks.h"

#include "Check.h"
#include "track_grid/Constants.h"

#include <string>

namespace TrackGridE2E
{

namespace
{

void checkWarmArm (Rig& rig, TrackGridResult& result, int slot,
                   const std::string& path)
{
    auto& session = rig.session();
    const auto state = session.getWarmState (slot);

    if (state == ihub::SessionWarmState::warming)
    {
        ++result.warmingSeen;
        E2E::check (false, path + " reports warming for track " + std::to_string (slot));
    }

    E2E::check (state == ihub::SessionWarmState::active,
                path + " leaves track " + std::to_string (slot) + " active");
    E2E::check (session.getWarmStateName (slot) != std::string ("warming"),
                path + " never names track " + std::to_string (slot) + " warming");
}

}

void runPadSwitchPass (Rig& rig, TrackGridResult& result)
{
    auto& session = rig.session();
    const auto creationBaseline = session.getWarmCreationCount();
    const auto evictionBaseline = session.getWarmEvictionCount();

    for (int index = 0; index < kTrackCount; ++index)
    {
        rig.press (trackPad (index));
        ++result.switches;

        E2E::check (rig.state().getArmedSlot() == index,
                    "track pad " + std::to_string (trackPad (index))
                        + " arms track " + std::to_string (index));

        checkWarmArm (rig, result, index, "the pad switch");

        result.lastSwitchRmsDbfs = rig.renderRms (kSettleBlocks, kRmsBlocks);
        E2E::check (result.lastSwitchRmsDbfs > kMinimumRmsDbfs,
                    "track " + std::to_string (index)
                        + " renders non-silent audio after the pad switch");

        rig.paintLeds();
    }

    E2E::check (session.getWarmCreationCount() == creationBaseline,
                "the pad pass creates no new warm instances");
    E2E::check (session.getWarmEvictionCount() == evictionBaseline,
                "the pad pass evicts nothing");
}

void runApiSwitchPass (Rig& rig, TrackGridResult& result)
{
    auto& session = rig.session();
    const auto creationBaseline = session.getWarmCreationCount();
    const auto evictionBaseline = session.getWarmEvictionCount();

    for (int index = 0; index < kTrackCount; ++index)
    {
        const auto path = "/tracks/" + std::to_string (index) + "/select";
        const auto response = rig.client().Post (path.c_str(), "{}", "application/json");

        ++result.switches;

        E2E::check (response && response->status == 200,
                    "POST " + path + " returns 200");

        E2E::check (rig.state().getArmedSlot() == index,
                    "the API arms track " + std::to_string (index));

        const auto snapshot = rig.state().snapshot();
        E2E::check (snapshot.tracks.size() == static_cast<std::size_t> (kTrackCount)
                        && snapshot.tracks[static_cast<std::size_t> (index)].selected,
                    "the API reports track " + std::to_string (index) + " selected");

        checkWarmArm (rig, result, index, "the API switch");

        result.lastSwitchRmsDbfs = rig.renderRms (kSettleBlocks, kRmsBlocks);
        E2E::check (result.lastSwitchRmsDbfs > kMinimumRmsDbfs,
                    "track " + std::to_string (index)
                        + " renders non-silent audio after the API switch");

        rig.paintLeds();
    }

    E2E::check (session.getWarmCreationCount() == creationBaseline,
                "the API pass creates no new warm instances");
    E2E::check (session.getWarmEvictionCount() == evictionBaseline,
                "the API pass evicts nothing");

    result.creationAfterSwitches = session.getWarmCreationCount();
    result.evictionsAfterSwitches = session.getWarmEvictionCount();
}

}
