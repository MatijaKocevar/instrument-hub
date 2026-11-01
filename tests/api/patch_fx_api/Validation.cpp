#include "Validation.h"

#include "../JsonSupport.h"
#include "../TestSupport.h"

namespace ApiTest::PatchFxApi
{

void runValidation (SessionRig& rig)
{
    auto& client = rig.client();

    auto response = client.Post ("/patches/0/fx", R"({"type":"nonsense"})",
                                 "application/json");
    check (response && response->status == 400, "POST rejects an unknown effect type");

    response = client.Post ("/patches/0/fx", R"({"type":"eq","wet":1.5})",
                            "application/json");
    check (response && response->status == 400, "POST rejects an out-of-range wet value");

    response = client.Post ("/patches/0/fx", R"({"type":"eq","bogus":1})",
                            "application/json");
    check (response && response->status == 400, "POST rejects an unknown field");

    response = client.Get ("/patches/99/fx");
    check (response && response->status == 404, "GET rejects an out-of-range slot");

    response = client.Delete ("/patches/0/fx/0");
    check (response && response->status == 404, "DELETE rejects an out-of-range index");

    response = client.Patch ("/patches/0/fx/0", R"({"wet":0.5})",
                             "application/json");
    check (response && response->status == 404, "PATCH rejects an out-of-range index");

    response = client.Post ("/patches/0/fx/reorder", R"({"order":[0]})",
                            "application/json");
    check (response && response->status == 400,
           "reorder rejects an order that is not a permutation");
}

}
