#include "TrackSupport.h"

#include "../EventStream.h"
#include "../JsonSupport.h"
#include "../TestSupport.h"

#include "Session.h"

#include <string>

namespace ApiTest::SessionApi
{

void runTrackFx (SessionRig& rig,
                 httplib::ws::WebSocketClient& events,
                 bool eventsConnected)
{
    auto& session = rig.session();
    auto& client = rig.client();

    auto response = client.Post ("/tracks/2/fx/0/toggle", "{}", "application/json");
    auto state = response ? parseJson (response->body) : juce::var {};

    check (response && response->status == 200, "POST /tracks/2/fx/0/toggle returns 200");
    check (boolProperty (hotFxAt (trackAt (state, kTrack), 0), "bypassed"),
           "the hot FX toggle bypasses the chosen insert");

    AudioEngine::FxChainSnapshot chain;
    std::string error;
    check (session.getSlotFxChain (kTrack, chain, error),
           "the session still publishes the track FX chain");

    if (chain.inserts.size() > 0)
        check (chain.inserts[0].bypassed,
               "the bypass reached the audio session's first insert");

    if (eventsConnected)
    {
        juce::var frame;
        const bool sawTrack = readEventUntil (events, [&] (const juce::var& candidate)
        {
            return isTrackEvent (candidate, kTrack)
                   && boolProperty (hotFxAt (trackAt (property (candidate, "state"), kTrack),
                                             0), "bypassed");
        }, 2000, frame);

        check (sawTrack, "the WebSocket delivers the hot FX toggle");
    }

    response = client.Get ("/state");
    state = response ? parseJson (response->body) : juce::var {};
    check (boolProperty (hotFxAt (trackAt (state, kTrack), 0), "bypassed"),
           "the hot FX bypass persists across GET /state");

    response = client.Post ("/tracks/2/fx/0/toggle", "{}", "application/json");
    state = response ? parseJson (response->body) : juce::var {};
    check (response && response->status == 200
               && ! boolProperty (hotFxAt (trackAt (state, kTrack), 0), "bypassed"),
           "toggling the hot FX again clears the bypass");

    response = client.Post ("/tracks/2/fx/2/toggle", "{}", "application/json");
    check (response && response->status == 404,
           "the hot FX command rejects an out-of-range hot index");

    response = client.Post ("/tracks/2/fx/0/assign",
                            R"({"insertId":"comp"})",
                            "application/json");
    state = response ? parseJson (response->body) : juce::var {};

    check (response && response->status == 200,
           "POST /tracks/2/fx/0/assign returns 200");
    check (stringProperty (hotFxAt (trackAt (state, kTrack), 0), "name")
               == "Compressor",
           "the assignment publishes the new insert name");
    check (stringProperty (hotFxAt (trackAt (state, kTrack), 1), "id").empty(),
           "assigning the other insert clears its previous hot slot");

    response = client.Get ("/state");
    state = response ? parseJson (response->body) : juce::var {};
    check (stringProperty (hotFxAt (trackAt (state, kTrack), 0), "id") == "comp",
           "the hot FX assignment persists across GET /state");

    response = client.Post ("/tracks/2/fx/0/assign",
                            R"({"insertId":"missing"})",
                            "application/json");
    check (response && response->status == 400,
           "an insert outside the chain is rejected");

    response = client.Post ("/tracks/2/fx/0/assign",
                            R"({"insertId":"eq"})",
                            "application/json");
    state = response ? parseJson (response->body) : juce::var {};
    check (response && response->status == 200
               && stringProperty (hotFxAt (trackAt (state, kTrack), 0), "id")
                      == "eq",
           "the hot FX assignment can move back to the first insert");

    response = client.Post ("/tracks/2/fx/2/assign",
                            R"({"insertId":"eq"})",
                            "application/json");
    check (response && response->status == 404,
           "the assignment rejects an out-of-range hot index");
}

}
