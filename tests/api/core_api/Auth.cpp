#include "Auth.h"

#include "../TestSupport.h"

namespace ApiTest::CoreApi
{

void runAuth (Rig& rig)
{
    auto& client = rig.http();
    auto& server = rig.server;

    server.setAuthToken ("test-token");

    auto response = client.Get ("/state");
    check (response && response->status == 401, "auth enabled: missing token gets 401");

    response = client.Get ("/state",
                           httplib::Headers { { "X-Instrument-Hub-Token", "test-token" } });
    check (response && response->status == 200, "auth enabled: header token gets 200");

    response = client.Get ("/state",
                           httplib::Headers { { "Authorization", "Bearer test-token" } });
    check (response && response->status == 200, "auth enabled: bearer token gets 200");

    server.setAuthToken ("");
}

} // namespace ApiTest::CoreApi
