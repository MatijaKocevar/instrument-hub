#include "TrackSupport.h"

#include "../EventStream.h"
#include "../JsonSupport.h"
#include "../TestSupport.h"

#include "Session.h"

#include <cmath>
#include <string>

namespace ApiTest::SessionApi
{

void runTrackVolume (SessionRig& rig,
                     httplib::ws::WebSocketClient& events,
                     bool eventsConnected)
{
    auto& session = rig.session();
    auto& client = rig.client();

    for (int step = 0; step <= 4; ++step)
    {
        auto response = client.Post ("/tracks/2/volume",
                                     "{\"step\":" + std::to_string (step) + "}",
                                     "application/json");
        const auto state = response ? parseJson (response->body) : juce::var {};

        check (response && response->status == 200,
               "POST /tracks/2/volume step " + std::to_string (step) + " returns 200");

        const auto track = trackAt (state, kTrack);
        const double gain = expectedGain (step);

        check (intProperty (track, "volumeStep") == step,
               "volume step " + std::to_string (step) + " is published");
        check (std::abs (numberProperty (track, "gain") - gain) < 1.0e-6,
               "volume step " + std::to_string (step) + " maps to the documented gain");
        check (std::abs (session.getEngine().getSlot (kTrack).getGain() - gain) < 1.0e-4,
               "volume step " + std::to_string (step) + " reaches the audio slot");

        if (eventsConnected)
        {
            juce::var frame;
            const bool sawTrack = readEventUntil (
                events,
                [&] (const juce::var& candidate)
                {
                    return isTrackEvent (candidate, kTrack)
                           && intProperty (trackAt (property (candidate, "state"), kTrack),
                                           "volumeStep") == step;
                },
                2000, frame);

            check (sawTrack, "the WebSocket delivers volume step " + std::to_string (step));
        }
    }

    auto response = client.Get ("/state");
    const auto state = response ? parseJson (response->body) : juce::var {};

    check (response && response->status == 200, "GET /state returns 200 after volume");
    check (intProperty (trackAt (state, kTrack), "volumeStep") == 4
               && std::abs (numberProperty (trackAt (state, kTrack), "volumeDb") - 3.0)
                      < 1.0e-6,
           "the +3 dB step persists across GET /state");

    response = client.Post ("/tracks/2/volume", "{\"step\":5}", "application/json");
    check (response && response->status == 400,
           "the volume command rejects step 5");

    response = client.Post ("/tracks/2/volume", "{}", "application/json");
    check (response && response->status == 400,
           "the volume command requires step");
}

}
