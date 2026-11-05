#include "Capture.h"

#include "../TestSupport.h"

#include "Fixture.h"

#include "controller/LaunchpadProtocol.h"

#include <cmath>
#include <string>

namespace TrackPadTest
{

using ControllerTest::check;

namespace
{

bool near (double left, double right)
{
    return std::abs (left - right) < 1.0e-6;
}

const Api::TrackState& trackAt (const Api::StateSnapshot& snapshot, int index)
{
    return snapshot.tracks[static_cast<std::size_t> (index)];
}

}

void runCaptureChecks()
{
    Fixture fixture;
    std::string error;

    check (fixture.load (error),
           std::string ("shared/tracks-set.yaml loads")
               + (error.empty() ? std::string() : " (" + error + ")"));

    fixture.paint();
    check (fixture.redPads() == 0, "no track is red before the first press");

    fixture.press (57);
    fixture.release (57);

    auto snapshot = fixture.state->snapshot();
    check (fixture.state->getArmedSlot() == 1 && trackAt (snapshot, 1).selected,
           "pressing track pad 57 arms track 2");
    check (! trackAt (snapshot, 0).selected && ! trackAt (snapshot, 7).selected,
           "arming a track deselects the others");

    fixture.paint();
    check (fixture.redPads() == 1
               && fixture.driver.getPadLed (57).velocity
                      == Launchpad::velocityFor (Launchpad::Colour::Red),
           "exactly one track LED is red, on the pressed track");
    check (fixture.driver.getPadLed (56).velocity
               == Launchpad::velocityForTrackColour (0),
           "the other track pads keep their instrument colours");

    fixture.press (48);
    fixture.release (48);

    snapshot = fixture.state->snapshot();
    check (! fixture.toggledInserts.empty()
               && fixture.toggledInserts.back() == "eq"
               && trackAt (snapshot, 0).hotFx[0].bypassed,
           "fx pad 48 toggles the first hot insert on track 1");

    fixture.paint();
    check (fixture.driver.getPadLed (48).velocity
               == Launchpad::velocityFor (Launchpad::Colour::Yellow),
           "a bypassed hot insert LED shows disabled");

    fixture.press (48);
    fixture.release (48);
    snapshot = fixture.state->snapshot();
    check (! trackAt (snapshot, 0).hotFx[0].bypassed,
           "pressing the fx pad again re-enables the insert");

    fixture.paint();
    check (fixture.driver.getPadLed (48).velocity
               == Launchpad::velocityFor (Launchpad::Colour::Green),
           "an enabled hot insert LED shows enabled");

    fixture.press (40);
    fixture.release (40);
    snapshot = fixture.state->snapshot();
    check (! fixture.toggledInserts.empty()
               && fixture.toggledInserts.back() == "comp"
               && trackAt (snapshot, 0).hotFx[1].bypassed,
           "fx pad 40 toggles the second hot insert on track 1");

    fixture.paint();
    check (fixture.driver.getPadLed (49).velocity
               == Launchpad::velocityFor (Launchpad::Colour::Off),
           "an unassigned hot pad stays dark");

    fixture.press (16);
    fixture.release (16);
    snapshot = fixture.state->snapshot();
    check (trackAt (snapshot, 0).volumeStep == 2
               && ! fixture.gains.empty()
               && fixture.gains.back().slot == 0
               && near (fixture.gains.back().gain, 0.75),
           "volume pad 16 sets track 1 to 75% (gain 0.75)");

    fixture.paint();
    check (fixture.driver.getPadLed (16).velocity == 0x13,
           "the 75% volume pad is lit in its green shade");

    check (fixture.state->getArmedSlot() == 1,
           "fx and volume pads do not re-arm the selected track");

    fixture.press (8);
    fixture.release (8);
    snapshot = fixture.state->snapshot();
    check (trackAt (snapshot, 0).volumeStep == 1
               && near (fixture.gains.back().gain, 0.5),
           "volume pad 8 sets track 1 to 50% (gain 0.5)");

    fixture.press (24);
    fixture.release (24);
    snapshot = fixture.state->snapshot();
    check (trackAt (snapshot, 0).volumeStep == 3
               && near (fixture.gains.back().gain, 1.0),
           "volume pad 24 sets track 1 to 100% (unity gain)");

    fixture.paint();
    check (fixture.driver.getPadLed (24).velocity == 0x18,
           "the 100% pad lights in a lighter green");

    fixture.press (32);
    fixture.release (32);
    snapshot = fixture.state->snapshot();
    check (trackAt (snapshot, 0).volumeStep == 4
               && near (fixture.gains.back().gain, 1.4125375446227544),
           "volume pad 32 applies the +3 dB boost step");

    fixture.paint();
    check (fixture.driver.getPadLed (32).velocity == 0x20,
           "the +3 dB step uses the lightest green LED");

    fixture.press (63);
    fixture.release (63);
    fixture.paint();
    snapshot = fixture.state->snapshot();
    check (fixture.state->getArmedSlot() == 7
               && trackAt (snapshot, 7).selected
               && fixture.redPads() == 1
               && fixture.driver.getPadLed (63).velocity
                      == Launchpad::velocityFor (Launchpad::Colour::Red)
               && fixture.driver.getPadLed (57).velocity
                      == Launchpad::velocityForTrackColour (1),
           "switching tracks moves the single red LED to track 8");
}

}
