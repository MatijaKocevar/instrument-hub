#include "TrackLayout.h"

#include "Support.h"

#include "api/state/TrackState.h"
#include "app/project/TrackLayoutSave.h"
#include "controller/MappingYaml.h"

#include <string>
#include <vector>

namespace ProjectTest
{

namespace
{

std::vector<Api::TrackConfig> makeTracks()
{
    std::vector<Api::TrackConfig> tracks;

    for (int index = 0; index < 8; ++index)
    {
        Api::TrackConfig track;
        track.id = "track-" + std::to_string (index + 1);
        track.name = "Track " + std::to_string (index + 1);
        track.slot = index;
        track.volumeStep = index % 6;
        tracks.push_back (std::move (track));
    }

    tracks[0].hotFx = { "eq", "comp" };
    tracks[1].hotFx = { "", "delay" };
    tracks[2].hotFx = { "reverb" };
    return tracks;
}

const Controller::TrackSpec* findTrack (const Controller::SetMapping& mapping,
                                        const std::string& id)
{
    for (const auto& track : mapping.tracks)
        if (track.id == id)
            return &track;

    return nullptr;
}

}

void runTrackLayoutChecks()
{
    const auto text = bundledTracksSetFile().loadFileAsString().toStdString();
    check (! text.empty(), "the track template text is available");

    std::vector<Api::TrackConfig> tracks;
    juce::String error;

    std::string unchanged;
    check (ihub::app::saveTrackLayoutToMapping (text, tracks, unchanged, error),
           "an empty track list keeps the mapping: " + error.toStdString());
    check (unchanged == text,
           "a track project without hot FX keeps mapping.yaml byte-identical");

    tracks = makeTracks();

    std::string updated;
    check (ihub::app::saveTrackLayoutToMapping (text, tracks, updated, error),
           "the track hot FX list saves: " + error.toStdString());
    check (updated != text, "the mapping changes when hot FX is assigned");

    const auto pads = text.find ("pads:");
    check (pads != std::string::npos && updated.size() > pads
               && updated.substr (updated.find ("pads:")) == text.substr (pads),
           "saving hot FX leaves the pad bindings byte-identical");

    Controller::SetMapping mapping;
    Controller::MappingError mappingError;
    check (Controller::parseSetMappingYaml (updated, mapping, mappingError),
           "the saved mapping parses: " + mappingError.message);

    const auto* first = findTrack (mapping, "track-1");
    const auto* second = findTrack (mapping, "track-2");
    const auto* third = findTrack (mapping, "track-3");
    const auto* fourth = findTrack (mapping, "track-4");

    check (first != nullptr && first->hotFx.size() == 2
               && first->hotFx[0] == "eq" && first->hotFx[1] == "comp",
           "the first track keeps two hot FX entries");
    check (second != nullptr && second->hotFx.size() == 2
               && second->hotFx[0].empty() && second->hotFx[1] == "delay",
           "a gap between hot FX slots survives the round-trip");
    check (third != nullptr && third->hotFx.size() == 1
               && third->hotFx[0] == "reverb",
           "a single hot FX entry is written");
    check (fourth != nullptr && fourth->hotFx.empty(),
           "tracks without hot FX stay empty");

    std::string rewritten;
    check (ihub::app::saveTrackLayoutToMapping (updated, tracks, rewritten, error),
           "saving the same track layout twice succeeds: " + error.toStdString());
    check (rewritten == updated, "the track mapping update is idempotent");

    tracks[0].hotFx.clear();
    tracks[1].hotFx.clear();
    tracks[2].hotFx.clear();

    std::string cleared;
    check (ihub::app::saveTrackLayoutToMapping (updated, tracks, cleared, error),
           "clearing the hot FX list saves: " + error.toStdString());
    check (cleared.find ("hotFx:") == std::string::npos,
           "clearing every hot FX removes the property");
}

}
