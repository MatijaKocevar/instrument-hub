#include "Account.h"

#include "Fixtures.h"

#include "../JsonSupport.h"
#include "../TestSupport.h"

#include <string>

namespace ApiTest::AppSettings
{

namespace
{
bool hasPairingToken (const juce::var& document)
{
    const auto account = property (document, "account");
    const auto* object = account.getDynamicObject();
    return object != nullptr && object->hasProperty ("pairingToken");
}
}

void runAccountPersistence()
{
    Fixture fixture;
    const bool started = fixture.start();
    check (started, "the account persistence fixture starts");

    if (! started)
        return;

    auto& client = fixture.http();

    auto response = client.Post ("/pairing/lan", "{\"enabled\":true}",
                                 "application/json");
    check (response && response->status == 200,
           "the account test enables LAN mode");
    check (fixture.waitForStateStatus (401),
           "LAN mode takes effect before the account checks continue");

    response = client.Get ("/pairing");
    check (response && response->status == 200,
           "GET /pairing answers after enabling LAN mode");

    std::string code;
    std::string token;

    if (response && response->status == 200)
    {
        const auto body = parseJson (response->body);
        code = stringProperty (body, "code");
        check (code.size() == 6, "the account test reads a pairing code");
    }

    response = client.Post ("/pair",
                            "{\"code\":\"" + code + "\",\"clientName\":\"Tablet\"}",
                            "application/json");
    check (response && response->status == 200,
           "the account test pairs a client");

    if (response && response->status == 200)
    {
        const auto body = parseJson (response->body);
        token = stringProperty (body, "token");
        check (token.size() == 64, "the account test reads a pairing token");
    }

    check (isOwnerOnly (fixture.file()),
           "the settings file is owner-only after the token save");

    auto document = fixture.document();
    auto account = property (document, "account");

    check (boolProperty (account, "lanEnabled"),
           "the settings file stores LAN mode on");
    check (stringProperty (account, "pairingToken") == token,
           "the settings file stores the pairing token");

    check (fixture.restart(), "the account fixture restarts");
    check (fixture.api().isLanModeEnabled(),
           "LAN mode returns after the restart");

    response = fixture.http().Get (
        "/state", httplib::Headers { { "X-Instrument-Hub-Token", token } });
    check (response && response->status == 200,
           "the stored pairing token is accepted after the restart");

    response = fixture.http().Get ("/state");
    check (response && response->status == 401,
           "restored LAN mode still rejects an unpaired client");

    response = fixture.http().Post ("/pairing/revoke", "", "application/json");
    check (response && response->status == 200,
           "the account test revokes every pairing token");

    document = fixture.document();
    check (! hasPairingToken (document),
           "revoking removes the token from the settings file");
    check (boolProperty (property (document, "account"), "lanEnabled"),
           "revoking keeps the LAN mode setting");

    check (fixture.restart(), "the account fixture restarts after revocation");

    response = fixture.http().Get (
        "/state", httplib::Headers { { "X-Instrument-Hub-Token", token } });
    check (response && response->status == 401,
           "a revoked token is rejected after the restart");

    response = fixture.http().Post ("/pairing/lan", "{\"enabled\":false}",
                                    "application/json");
    check (response && response->status == 200,
           "the account test disables LAN mode");
    check (fixture.waitForStateStatus (200),
           "disabling LAN mode restores unauthenticated loopback access");

    document = fixture.document();
    account = property (document, "account");
    check (! boolProperty (account, "lanEnabled"),
           "the settings file stores LAN mode off");
    check (! hasPairingToken (document),
           "LAN mode off leaves no pairing token in the settings file");

    check (fixture.restart(), "the account fixture restarts after LAN mode off");
    check (! fixture.api().isLanModeEnabled(),
           "LAN mode off returns after the restart");

    response = fixture.http().Get ("/state");
    check (response && response->status == 200,
           "loopback access works without a token after the restart");
}

}
