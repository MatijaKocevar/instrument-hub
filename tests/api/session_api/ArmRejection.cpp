#include "ArmRejection.h"

#include "../TestSupport.h"

namespace ApiTest::SessionApi
{

void runArmRejection (SessionRig& rig)
{
    auto response = rig.client().Post ("/arm", "{\"slot\":99}", "application/json");
    check (response && response->status == 400, "POST /arm rejects an out-of-range slot");
}

}
