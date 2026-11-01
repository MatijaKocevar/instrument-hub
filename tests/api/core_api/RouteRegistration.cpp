#include "RouteRegistration.h"

#include "../TestSupport.h"

#include <string>

namespace ApiTest::CoreApi
{

void runRouteRegistration (Rig& rig)
{
    auto& client = rig.http();

    struct RouteProbe
    {
        const char* method;
        const char* path;
        const char* body;
    };

    const RouteProbe probes[] = {
        { "POST", "/arm", "{}" },
        { "PATCH", "/patches/0", "{}" },
        { "POST", "/patches/0/source", "{}" },
        { "POST", "/patches/0/editor", nullptr },
        { "DELETE", "/patches/0/editor", nullptr },
        { "POST", "/patches/0/state/capture", nullptr },
        { "GET", "/patches/0/fx", nullptr },
        { "POST", "/patches/0/fx", "{}" },
        { "PUT", "/patches/0/fx", "{}" },
        { "PATCH", "/patches/0/fx/0", "{}" },
        { "DELETE", "/patches/0/fx/0", nullptr },
        { "POST", "/patches/0/fx/reorder", "{}" },
        { "GET", "/settings", nullptr },
        { "POST", "/settings", "{}" },
        { "GET", "/transport", nullptr },
        { "GET", "/audio", nullptr },
        { "POST", "/audio", "{}" },
        { "POST", "/audio/measure", nullptr },
        { "GET", "/plugins", nullptr },
        { "POST", "/plugins/rescan", nullptr },
        { "GET", "/midi", nullptr },
        { "POST", "/midi/inputs", "{}" },
        { "POST", "/midi/pad-test", nullptr },
        { "GET", "/pairing", nullptr },
        { "POST", "/pairing/lan", "{}" },
        { "POST", "/pairing/revoke", nullptr },
        { "POST", "/pair", "{}" },
    };

    for (const auto& probe : probes)
    {
        const std::string method (probe.method);
        const std::string path (probe.path);
        const std::string body = probe.body != nullptr ? probe.body : std::string();
        httplib::Result result;

        if (method == "GET")
            result = client.Get (path);
        else if (method == "POST")
            result = client.Post (path, body, "application/json");
        else if (method == "PUT")
            result = client.Put (path, body, "application/json");
        else if (method == "PATCH")
            result = client.Patch (path, body, "application/json");
        else if (method == "DELETE")
            result = client.Delete (path);

        check (result && result->status != 404,
               method + " " + path + " is registered");
    }
}

}
