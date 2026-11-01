#include "Settings.h"

#include "../JsonSupport.h"
#include "../TestSupport.h"

#include <cmath>

namespace ApiTest::CoreApi
{

void runSettings (Rig& rig)
{
    auto& client = rig.http();

    auto response = client.Get ("/settings");
    check (response && response->status == 200, "GET /settings returns 200");

    response = client.Post ("/settings", "{\"masterGain\":0.5}", "application/json");
    check (response && response->status == 200, "POST /settings returns 200");

    if (response && response->status == 200)
    {
        const auto body = parseJson (response->body);
        check (std::abs (numberProperty (body, "masterGain") - 0.5) < 1.0e-9,
               "masterGain updated to 0.5");
    }

    check (std::abs (rig.state.getSettings().masterGain - 0.5) < 1.0e-9,
           "state store holds masterGain 0.5");

    response = client.Post ("/settings", "{\"masterGain\":5.0}", "application/json");
    check (response && response->status == 400, "POST /settings rejects an out-of-range value");

    response = client.Post ("/settings", "{\"nonsense\":1}", "application/json");
    check (response && response->status == 400, "POST /settings rejects an unknown field");
}

} // namespace ApiTest::CoreApi
