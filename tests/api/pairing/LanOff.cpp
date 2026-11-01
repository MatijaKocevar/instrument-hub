#include "LanOff.h"

#include "../JsonSupport.h"
#include "../TestSupport.h"

namespace ApiTest::Pairing
{

void runLanOff (Fixture& fixture)
{
    auto& client = fixture.http();

    auto response = client.Get ("/state");
    check (response && response->status == 200,
           "LAN off: unpaired GET /state returns 200 on loopback");

    response = client.Get ("/pairing");
    check (response && response->status == 200, "GET /pairing returns 200 on loopback");

    if (response && response->status == 200)
    {
        const auto body = parseJson (response->body);
        check (! boolProperty (body, "lanEnabled"),
               "pairing state reports LAN mode off by default");
        check (stringProperty (body, "code").empty(),
               "no pairing code is issued while LAN mode is off");
        check (stringProperty (body, "payload").empty(),
               "no QR payload is issued while LAN mode is off");
    }

    response = client.Post ("/pair", "{\"code\":\"123456\"}", "application/json");
    check (response && response->status == 403,
           "POST /pair is refused while LAN mode is off");

    response = client.Post ("/pairing/lan", "{}", "application/json");
    check (response && response->status == 400,
           "POST /pairing/lan requires an \"enabled\" field");

    response = client.Post ("/pairing/lan", "{\"enabled\":\"yes\"}", "application/json");
    check (response && response->status == 400,
           "POST /pairing/lan rejects a non-boolean toggle");
}

}
