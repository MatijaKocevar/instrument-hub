#include "PatchMix.h"

#include "../EventStream.h"
#include "../JsonSupport.h"
#include "../TestSupport.h"

#include <httplib.h>

#include <cmath>

namespace ApiTest::SessionApi
{

void runPatchMix (SessionRig& rig)
{
    auto& session = rig.session();
    auto& client = rig.client();

    auto patchResponse = client.Get ("/state");

    if (patchResponse && patchResponse->status == 200)
    {
        const auto body = parseJson (patchResponse->body);
        const auto patch = patchForSlot (body, 1);

        check (patch.isObject(), "GET /state publishes slot 1 in the patch tree");
        check (property (patch, "instrument").isObject(),
               "the patch tree carries the instrument descriptor");
        check (property (patch, "warmState").toString() == "active",
               "the patch tree carries the live warm state");
        check (intProperty (patch, "latencySamples") >= 0
                   && property (patch, "latencyMs").isDouble(),
               "the patch tree carries chain latency");
        check (property (patch, "volumeDb").isDouble()
                   && property (patch, "pan").isDouble()
                   && property (patch, "mute").isBool()
                   && property (patch, "solo").isBool()
                   && property (patch, "colorIndex").isInt(),
               "the patch tree carries volume, pan, mute, solo and colour");
    }
    else
    {
        check (false, "GET /state returns 200 for the patch tree");
    }

    httplib::ws::WebSocketClient events ("ws://127.0.0.1:"
                                         + std::to_string (rig.port())
                                         + "/events");
    events.set_connection_timeout (2, 0);
    events.set_read_timeout (1, 0);
    const auto eventsConnected = events.connect();
    check (static_cast<bool> (eventsConnected), "WebSocket /events connects");

    patchResponse = client.Patch ("/patches/1",
                                  "{\"volumeDb\":-12,\"colorIndex\":5}",
                                  "application/json");
    check (patchResponse && patchResponse->status == 200,
           "PATCH /patches/1 returns 200");

    const float expectedGain = static_cast<float> (std::pow (10.0, -12.0 / 20.0));
    check (std::abs (session.getEngine().getSlot (1).getGain() - expectedGain) < 0.001f,
           "PATCH volumeDb applies the linear gain to the audio slot");

    if (patchResponse && patchResponse->status == 200)
    {
        const auto body = parseJson (patchResponse->body);
        const auto patch = patchForSlot (body, 1);

        check (std::abs (numberProperty (patch, "volumeDb") + 12.0) < 1.0e-9,
               "PATCH volumeDb is published in the patch tree");
        check (intProperty (patch, "colorIndex") == 5,
               "PATCH colorIndex is published in the patch tree");
    }

    if (eventsConnected)
    {
        juce::var event;

        const bool sawPatch = readEventUntil (
            events,
            [] (const juce::var& frame)
            {
                return property (property (frame, "event"), "kind").toString() == "patch"
                       && intProperty (property (frame, "event"), "slot") == 1;
            },
            2000, event);

        check (sawPatch, "the /events WebSocket delivers the patch change");

        if (sawPatch)
        {
            const auto patch = patchForSlot (property (event, "state"), 1);
            check (std::abs (numberProperty (patch, "volumeDb") + 12.0) < 1.0e-9,
                   "the event frame carries the new patch volume");
            check (intProperty (patch, "colorIndex") == 5,
                   "the event frame carries the new patch colour");
        }
    }

    patchResponse = client.Patch ("/patches/1", "{\"mute\":true}",
                                  "application/json");
    check (patchResponse && patchResponse->status == 200,
           "PATCH mute returns 200");
    check (session.getEngine().getSlot (1).getGain() < 1.0e-6f,
           "PATCH mute silences the audio slot");

    patchResponse = client.Patch ("/patches/1",
                                  "{\"mute\":false,\"volumeDb\":0,\"colorIndex\":0}",
                                  "application/json");
    check (patchResponse && patchResponse->status == 200,
           "PATCH restores the patch mix");

    patchResponse = client.Patch ("/patches/999", "{\"volumeDb\":-3}",
                                  "application/json");
    check (patchResponse && patchResponse->status == 404,
           "PATCH rejects an out-of-range patch slot");

    patchResponse = client.Patch ("/patches/1", "{\"colour\":3}",
                                  "application/json");
    check (patchResponse && patchResponse->status == 400,
           "PATCH rejects an unknown patch field");

    patchResponse = client.Patch ("/patches/1", "{\"colorIndex\":99}",
                                  "application/json");
    check (patchResponse && patchResponse->status == 400,
           "PATCH rejects an out-of-range colour");

    events.close();
}

} // namespace ApiTest::SessionApi
