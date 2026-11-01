#include "Transport.h"

#include "../AudioMeasurements.h"
#include "../JsonSupport.h"
#include "../TestSupport.h"

#include <cmath>

namespace ApiTest::SessionApi
{

void runTransport (SessionRig& rig)
{
    auto& session = rig.session();
    auto& client = rig.client();

    auto response = client.Post ("/transport",
                                 "{\"playing\":true,\"bpm\":120,\"positionSamples\":0}",
                                 "application/json");
    check (response && response->status == 200, "POST /transport start returns 200");

    renderBlocks (session, 10);

    auto transportResponse = client.Get ("/transport");

    if (transportResponse && transportResponse->status == 200)
    {
        const auto body = parseJson (transportResponse->body);
        check (boolProperty (body, "playing"),
               "GET /transport reports the session transport playing");
        check (static_cast<juce::int64> (property (body, "positionSamples")) == 1280,
               "GET /transport reports 10 rendered 128-frame blocks (1280 samples)");
        check (std::abs (numberProperty (body, "ppq") - 1280.0 * 120.0
                                                      / (60.0 * kSampleRate))
                   < 1.0e-9,
               "GET /transport reports the matching PPQ position");
    }
    else
    {
        check (false, "GET /transport returns 200 while playing");
    }

    response = client.Post ("/transport", "{\"playing\":false}", "application/json");
    check (response && response->status == 200, "POST /transport stop returns 200");

    renderBlocks (session, 10);

    transportResponse = client.Get ("/transport");

    if (transportResponse && transportResponse->status == 200)
    {
        const auto body = parseJson (transportResponse->body);
        check (! boolProperty (body, "playing"),
               "GET /transport reports the session transport stopped");
        check (static_cast<juce::int64> (property (body, "positionSamples")) == 1280,
               "stopping the transport does not rewind the position");
    }
}

} // namespace ApiTest::SessionApi
