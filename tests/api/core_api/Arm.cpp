#include "Arm.h"

#include "../JsonSupport.h"
#include "../TestSupport.h"

#include <cmath>

namespace ApiTest::CoreApi
{

void runArm (Rig& rig)
{
    auto& client = rig.http();

    auto response = client.Post ("/arm", "{\"slot\":3}", "application/json");
    check (response && response->status == 200, "POST /arm returns 200");

    if (response && response->status == 200)
    {
        const auto body = parseJson (response->body);
        check (intProperty (body, "armedSlot") == 3,
               "POST /arm arms slot 3");
        check (rig.state.getArmedSlot() == 3, "state store reports slot 3 armed");

        const auto slots = property (body, "slots").getArray();
        const auto third = slots != nullptr ? (*slots)[3] : juce::var();
        check (third.isObject() && boolProperty (third, "armed"),
               "slot 3 is marked armed");
    }

    response = client.Post ("/arm", "{\"slot\":99}", "application/json");
    check (response && response->status == 400, "POST /arm rejects an out-of-range slot");

    response = client.Post ("/arm", "{\"slot\":null}", "application/json");
    check (response && response->status == 200, "POST /arm accepts null to disarm");
    check (rig.state.getArmedSlot() == -1, "state store disarmed after null");
}

}
