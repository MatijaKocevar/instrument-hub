#include "track_grid/Checks.h"

#include "Check.h"
#include "track_grid/Constants.h"

#include "controller/LaunchpadProtocol.h"

#include <string>

namespace TrackGridE2E
{

void checkTrackLeds (Rig& rig, TrackGridResult& result)
{
    rig.paintLeds();

    auto& driver = rig.launchpad();
    const auto snapshot = rig.state().snapshot();

    result.redTrackLeds = countRedTrackLeds (driver);
    result.ledEvents = static_cast<int> (rig.ledEvents().size());

    E2E::check (result.ledEvents > 0, "the track grid emitted an LED stream");
    E2E::check (result.redTrackLeds == 1,
                "exactly one track pad is red ("
                    + std::to_string (result.redTrackLeds) + " found)");
    E2E::check (ledIs (driver, trackPad (0), Launchpad::Colour::Red),
                "the selected track pad is red");

    if (snapshot.tracks.size() > 1)
        E2E::check (driver.getPadLed (trackPad (1)).velocity
                        == Launchpad::velocityForPatchColour (1),
                    "an unselected track pad keeps its instrument colour");

    E2E::check (ledIs (driver, hotPad (0, 0), Launchpad::Colour::Green),
                "the re-enabled hot FX pad is green");
    E2E::check (ledIs (driver, hotPad (1, 3), Launchpad::Colour::Yellow),
                "the bypassed hot FX pad is yellow");
    E2E::check (ledIs (driver, volumePad (1, 5), Launchpad::Colour::Blue),
                "the API volume step lights the matching pad blue");
    E2E::check (ledIs (driver, volumePad (4, 0), Launchpad::Colour::White),
                "the over step lights the 100% pad white");

    bool staticOnly = true;

    for (const auto& event : rig.ledEvents())
        staticOnly = staticOnly && event.mode == 0;

    E2E::check (staticOnly, "the track grid emits only static LED messages");
}

}
