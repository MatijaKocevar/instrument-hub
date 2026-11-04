#include "track_grid/Checks.h"

#include "Check.h"
#include "track_grid/Constants.h"

#include "Session.h"

#include <cmath>
#include <string>

namespace TrackGridE2E
{

namespace
{

const Api::TrackState* trackAt (const Api::StateSnapshot& snapshot, int index)
{
    if (index < 0 || index >= static_cast<int> (snapshot.tracks.size()))
        return nullptr;

    return &snapshot.tracks[static_cast<std::size_t> (index)];
}

bool hotBypassed (const Api::TrackState& track, int hot)
{
    return hot >= 0
           && hot < static_cast<int> (track.hotFx.size())
           && track.hotFx[static_cast<std::size_t> (hot)].bypassed;
}

double gainForStep (int step)
{
    static constexpr double gains[] { 0.0, 0.25, 0.5, 0.75, 1.0,
                                      1.9952623149688795 };
    return step >= 0 && step <= kVolumeOverStep ? gains[step] : 0.0;
}

void checkChainBypass (Rig& rig, int slot, int insertIndex, bool expected,
                       const std::string& what)
{
    AudioEngine::FxChainSnapshot chain;
    std::string error;

    const bool read = rig.session().getSlotFxChain (slot, chain, error);
    E2E::check (read, what + " reads the live FX chain");

    if (read)
        E2E::check (insertIndex < static_cast<int> (chain.inserts.size())
                        && chain.inserts[static_cast<std::size_t> (insertIndex)].bypassed
                               == expected,
                    what + " reaches the audio chain");
}

void checkVolume (Rig& rig, int trackIndex, int step, const std::string& what)
{
    const auto snapshot = rig.state().snapshot();
    const auto* track = trackAt (snapshot, trackIndex);

    E2E::check (track != nullptr && track->volumeStep == step,
                what + " publishes volume step " + std::to_string (step));

    if (track == nullptr)
        return;

    E2E::check (std::abs (track->gain - gainForStep (step)) < 1.0e-6,
                what + " publishes the documented gain");

    const auto& slot = rig.session().getEngine().getSlot (track->slot);
    E2E::check (std::abs (slot.getGain() - gainForStep (step)) < 1.0e-4,
                what + " reaches the audio slot");
}

}

void runTrackActions (Rig& rig, TrackGridResult& result)
{
    rig.press (trackPad (0));
    rig.renderBlocks (kSwitchBlocks);
    rig.paintLeds();

    E2E::check (rig.state().getArmedSlot() == 0,
                "the action pass starts with track 0 selected");

    rig.press (hotPad (0, 0));
    ++result.fxToggles;

    auto snapshot = rig.state().snapshot();
    const auto* first = trackAt (snapshot, 0);
    E2E::check (first != nullptr && hotBypassed (*first, 0),
                "the hot FX pad bypasses track 0's first insert");
    checkChainBypass (rig, 0, 0, true, "the hot FX pad toggle");
    rig.paintLeds();

    auto response = rig.client().Post ("/tracks/0/fx/0/toggle", "{}",
                                      "application/json");
    ++result.fxToggles;
    E2E::check (response && response->status == 200,
                "POST /tracks/0/fx/0/toggle returns 200");

    snapshot = rig.state().snapshot();
    first = trackAt (snapshot, 0);
    E2E::check (first != nullptr && ! hotBypassed (*first, 0),
                "the API re-enables track 0's first insert");
    checkChainBypass (rig, 0, 0, false, "the API hot FX toggle");
    rig.paintLeds();

    response = rig.client().Post ("/tracks/3/fx/1/toggle", "{}",
                                 "application/json");
    ++result.fxToggles;
    E2E::check (response && response->status == 200,
                "POST /tracks/3/fx/1/toggle returns 200");

    snapshot = rig.state().snapshot();
    const auto* third = trackAt (snapshot, 3);
    E2E::check (third != nullptr && hotBypassed (*third, 1),
                "the API bypasses track 3's second insert");
    checkChainBypass (rig, 3, 1, true, "the API hot FX toggle");
    rig.paintLeds();

    rig.press (volumePad (3, 0));
    ++result.volumeSets;
    rig.renderBlocks (kSwitchBlocks);
    checkVolume (rig, 0, 3, "the volume pad");
    rig.paintLeds();

    rig.press (volumePad (4, 0));
    ++result.volumeSets;
    checkVolume (rig, 0, 4, "the 100% volume pad");

    rig.press (volumePad (4, 0));
    ++result.volumeSets;
    checkVolume (rig, 0, kVolumeOverStep, "the repeated 100% volume pad");
    rig.renderBlocks (kSwitchBlocks);
    rig.paintLeds();

    response = rig.client().Post ("/tracks/5/volume", R"({"step":1})",
                                 "application/json");
    ++result.volumeSets;
    E2E::check (response && response->status == 200,
                "POST /tracks/5/volume returns 200");
    checkVolume (rig, 5, 1, "the API volume command");
    rig.renderBlocks (kSwitchBlocks);
    rig.paintLeds();
}

}
