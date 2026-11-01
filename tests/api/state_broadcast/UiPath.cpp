#include "UiPath.h"

#include "../EventStream.h"
#include "../JsonSupport.h"
#include "../TestSupport.h"

#include <algorithm>
#include <string>

namespace ApiTest::StateBroadcast
{

void runUiPath (Rig& rig)
{
    auto& desktop = *rig.desktop;
    auto& tablet = *rig.tablet;

    httplib::Client http ("127.0.0.1", rig.server.getPort());
    http.set_connection_timeout (2, 0);
    http.set_read_timeout (2, 0);

    const auto tappedAt = std::chrono::steady_clock::now();
    const auto response = http.Post ("/arm", "{\"slot\":5}", "application/json");

    const double postLatency = elapsedMs (tappedAt);

    check (response && response->status == 200, "POST /arm returns 200");

    juce::var armedDesktop;
    const auto armFound = readEventUntil (desktop.socket, [] (const juce::var& candidate)
    {
        return isEvent (candidate, "arm")
               && intProperty (property (candidate, "event"), "slot") == 5;
    }, 1000, armedDesktop);

    const double armLatency = elapsedMs (tappedAt);
    rig.worstPadLatencyMs = std::max (rig.worstPadLatencyMs, armLatency);

    check (armFound, "the UI arm event reached the desktop socket");

    if (armFound)
    {
        check (armLatency <= kLatencyBudgetMs,
               "the UI arm event arrived in " + std::to_string (armLatency)
                   + " ms (round trip incl. HTTP: " + std::to_string (postLatency)
                   + " ms)");

        const auto mirrored = property (armedDesktop, "state");
        check (intProperty (mirrored, "armedSlot") == 5,
               "the mirrored state shows slot 5 armed");

        const auto slots = property (mirrored, "slots");

        if (const auto* array = slots.getArray())
        {
            check (array->size() == 8
                       && boolProperty ((*array)[5], "armed"),
                   "slot 5 is the only newly armed slot in the frame");
        }
        else
        {
            check (false, "the mirrored state carries the slot list");
        }
    }

    juce::var armedTablet;
    const auto tabletArmFound = readEventUntil (tablet.socket, [] (const juce::var& candidate)
    {
        return isEvent (candidate, "arm")
               && intProperty (property (candidate, "event"), "slot") == 5;
    }, 1000, armedTablet);

    check (tabletArmFound && elapsedMs (tappedAt) <= kLatencyBudgetMs,
           "the UI arm event reached the tablet socket within the budget");
}

} // namespace ApiTest::StateBroadcast
