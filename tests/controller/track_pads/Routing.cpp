#include "Routing.h"

#include "../TestSupport.h"

#include "controller/launchpad/TrackPadRouting.h"

#include <string>

namespace TrackPadTest
{

using ControllerTest::check;

void runRoutingChecks()
{
    Controller::PadBinding track;
    track.role = Controller::PadRole::track;
    track.slot = 3;

    auto action = Controller::trackPadActionFor (track, 4);
    check (action.has_value()
               && action->kind == Controller::TrackPadActionKind::arm
               && action->slot == 3,
           "a track pad arms its slot");

    Controller::PadBinding fx;
    fx.role = Controller::PadRole::fx;
    fx.slot = 5;
    fx.hot = 1;

    action = Controller::trackPadActionFor (fx, 4);
    check (action.has_value()
               && action->kind == Controller::TrackPadActionKind::toggleFx
               && action->slot == 5
               && action->hot == 1,
           "an fx pad toggles its hot insert");

    fx.hot = 2;
    check (! Controller::trackPadActionFor (fx, 4).has_value(),
           "an fx pad without a valid hot index is ignored");

    Controller::PadBinding volume;
    volume.role = Controller::PadRole::volume;
    volume.slot = 2;
    volume.step = 2;

    action = Controller::trackPadActionFor (volume, 4);
    check (action.has_value()
               && action->kind == Controller::TrackPadActionKind::setVolume
               && action->slot == 2
               && action->step == 2,
           "a volume pad sets its own step");

    volume.step = 4;

    action = Controller::trackPadActionFor (volume, 1);
    check (action.has_value() && action->step == 4,
           "the 100% pad sets 100% from a lower step");

    action = Controller::trackPadActionFor (volume, 4);
    check (action.has_value() && action->step == 5,
           "pressing the 100% pad again applies the over step");

    action = Controller::trackPadActionFor (volume, 5);
    check (action.has_value() && action->step == 4,
           "pressing the over pad returns to 100%");

    volume.step = 0;
    action = Controller::trackPadActionFor (volume, 5);
    check (action.has_value() && action->step == 0,
           "a non-100% pad leaves the over state");

    Controller::PadBinding patch;
    patch.role = Controller::PadRole::patch;
    patch.slot = 7;
    check (Controller::padRoleArms (patch.role)
               && Controller::padRoleArms (track.role)
               && ! Controller::padRoleArms (fx.role)
               && ! Controller::padRoleArms (volume.role),
           "only patch and track roles arm their slot");
}

}
