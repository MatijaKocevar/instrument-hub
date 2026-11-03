#include "TrackDocument.h"

#include "../TestSupport.h"

#include "TrackFixtures.h"
#include "controller/Mapping.h"

#include <string>

namespace MappingTest
{

using ControllerTest::check;

namespace
{

int countRole (const Controller::SetMapping& mapping, Controller::PadRole role)
{
    int count = 0;

    for (const auto& entry : mapping.pads)
        if (entry.has_value() && entry->role == role)
            ++count;

    return count;
}

}

void runTrackDocumentChecks()
{
    Controller::SetMapping mapping;
    Controller::MappingError error;
    const bool parsed = Controller::parseSetMappingYaml (makeTracksYaml(), mapping, error);

    check (parsed, std::string ("a tracks template parses")
                       + (parsed ? std::string()
                                 : " (" + error.message + ":"
                                       + std::to_string (error.line) + ")"));

    if (! parsed)
        return;

    check (mapping.schemaVersion == 2, "the tracks template declares schemaVersion 2");
    check (mapping.layout == Controller::MappingLayout::tracks,
           "the tracks template selects the track layout");
    check (mapping.countTracks() == 8,
           "the tracks template declares eight tracks (got "
               + std::to_string (mapping.countTracks()) + ")");
    check (mapping.tracks.front().id == "track-1"
               && mapping.tracks.front().slot == 0
               && mapping.tracks.back().id == "track-8"
               && mapping.tracks.back().slot == 7,
           "tracks are stored in slot order");
    check (mapping.tracks.front().hotFx.size() == 2
               && mapping.tracks.front().hotFx[0] == "eq"
               && mapping.tracks.front().hotFx[1] == "comp",
           "a track carries up to two hot FX insert ids");
    check (mapping.tracks.back().hotFx.size() == 1
               && mapping.tracks.back().hotFx.front() == "delay",
           "hotFx accepts a single insert id");

    check (mapping.countPads() == 64,
           "the tracks template binds all 64 pads (got "
               + std::to_string (mapping.countPads()) + ")");
    check (countRole (mapping, Controller::PadRole::track) == 8,
           "the tracks template declares eight track roles");
    check (countRole (mapping, Controller::PadRole::fx) == 16,
           "the tracks template declares sixteen fx roles");
    check (countRole (mapping, Controller::PadRole::volume) == 40,
           "the tracks template declares forty volume roles");
    check (countRole (mapping, Controller::PadRole::patch) == 0,
           "the tracks template declares no family patch roles");

    const auto* trackPad = mapping.padBinding (56);
    check (trackPad != nullptr
               && trackPad->role == Controller::PadRole::track
               && trackPad->track == "track-1"
               && trackPad->slot == 0
               && trackPad->hot == -1
               && trackPad->step == -1,
           "pad 56 selects track-1 on slot 0");

    const auto* topTrackPad = mapping.padBinding (63);
    check (topTrackPad != nullptr && topTrackPad->track == "track-8"
               && topTrackPad->slot == 7,
           "pad 63 selects track-8 on slot 7");

    for (int column = 0; column < 8; ++column)
    {
        const auto* hotZero = mapping.padBinding (48 + column);
        const auto* hotOne = mapping.padBinding (40 + column);

        check (hotZero != nullptr
                   && hotZero->role == Controller::PadRole::fx
                   && hotZero->hot == 0
                   && hotZero->slot == column,
               "row 6 pad " + std::to_string (48 + column) + " toggles hot FX 0");

        check (hotOne != nullptr
                   && hotOne->role == Controller::PadRole::fx
                   && hotOne->hot == 1
                   && hotOne->slot == column,
               "row 5 pad " + std::to_string (40 + column) + " toggles hot FX 1");

        for (int step = 0; step <= 4; ++step)
        {
            const auto* volume = mapping.padBinding (step * 8 + column);

            check (volume != nullptr
                       && volume->role == Controller::PadRole::volume
                       && volume->step == step
                       && volume->slot == column,
                   "row " + std::to_string (step) + " column " + std::to_string (column)
                       + " selects volume step " + std::to_string (step));
        }
    }

    check (mapping.trackAtSlot (2) != nullptr && mapping.trackAtSlot (2)->id == "track-3",
           "trackAtSlot finds a track by its slot");
    check (mapping.trackForId ("track-5") != nullptr
               && mapping.trackForId ("track-5")->slot == 4,
           "trackForId finds a track by its id");
    check (trackPad != nullptr && mapping.trackFor (*trackPad) == &mapping.tracks.front(),
           "trackFor resolves a pad binding to its track");

    Controller::SetMapping withEmptySections;
    check (Controller::parseSetMappingYaml (makeTracksYaml (true), withEmptySections, error),
           "a tracks template with empty families, macros and encoders parses");
    check (withEmptySections.families.empty()
               && withEmptySections.macros.empty()
               && withEmptySections.countEncoders() == 0,
           "the empty sections stay empty");
}

}
