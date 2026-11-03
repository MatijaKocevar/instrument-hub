#include "InitialState.h"

#include "../JsonSupport.h"
#include "../TestSupport.h"

namespace ApiTest::StateBroadcast
{

void runInitialState (Rig& rig)
{
    check (rig.desktop->connect(), "desktop WebSocket /events connects");
    check (rig.tablet->connect(), "tablet WebSocket /events connects");

    juce::var initialDesktop;
    juce::var initialTablet;

    check (rig.desktop->readInitial (initialDesktop),
           "desktop receives the initial state frame");
    check (rig.tablet->readInitial (initialTablet),
           "tablet receives the initial state frame");

    if (initialDesktop.isObject())
    {
        check (isEvent (initialDesktop, "initial"),
               "the first frame is marked initial");

        const auto pads = property (initialDesktop, "pads");

        check (pads.isArray() && pads.getArray()->size() == 64,
               "the frame mirrors all 64 pads");

        const auto mirrored = property (initialDesktop, "state");
        check (mirrored.isObject()
                   && intProperty (mirrored, "armedSlot") == -1,
               "the frame embeds the full /state document (nothing armed)");
        check (! property (mirrored, "tracks").isArray(),
               "the legacy family state carries no track list");
    }
}

}
