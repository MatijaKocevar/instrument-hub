#include "State.h"

#include "../JsonSupport.h"
#include "../TestSupport.h"

#include <cmath>

namespace ApiTest::CoreApi
{

void runState (Rig& rig)
{
    auto& client = rig.http();

    auto response = client.Get ("/state");
    check (response && response->status == 200, "GET /state returns 200");

    if (response && response->status == 200)
    {
        const auto body = parseJson (response->body);
        check (body.isObject(), "GET /state body is a JSON object");
        check (intProperty (body, "schemaVersion") == 1,
               "state schemaVersion is 1");
        check (intProperty (body, "armedSlot") == -1,
               "initial armedSlot is -1");

        const auto slots = property (body, "slots");

        if (const auto* array = slots.getArray())
        {
            check (array->size() == 8, "state lists 8 slots");

            const auto third = array->size() > 2 ? (*array)[2] : juce::var();
            check (third.isObject()
                       && intProperty (third, "latencySamples") == 240,
                   "GET /state publishes the slot's 240-sample chain latency");
            check (third.isObject()
                       && std::abs (numberProperty (third, "latencyMs") - 5.0)
                              < 1.0e-9,
                   "GET /state publishes the same chain latency as 5 ms");
        }
        else
        {
            check (false, "state slots is an array");
        }
    }
}

}
