#include "Transport.h"

#include "../JsonSupport.h"
#include "../TestSupport.h"

#include <cmath>

namespace ApiTest::CoreApi
{

void runTransport (Rig& rig)
{
    auto& client = rig.http();

    auto response = client.Post ("/transport", "{\"playing\":true,\"bpm\":128}",
                                 "application/json");
    check (response && response->status == 200, "POST /transport returns 200");

    if (response && response->status == 200)
    {
        const auto body = parseJson (response->body);
        check (boolProperty (body, "playing"), "transport is playing");
        check (std::abs (numberProperty (body, "bpm") - 128.0) < 1.0e-9,
               "transport bpm is 128");
    }

    response = client.Post ("/transport", "{\"bpm\":1000}", "application/json");
    check (response && response->status == 400, "POST /transport rejects an out-of-range bpm");
}

}
