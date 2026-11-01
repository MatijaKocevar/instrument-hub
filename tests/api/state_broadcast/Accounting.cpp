#include "Accounting.h"

#include "../TestSupport.h"

#include <iostream>
#include <string>

namespace ApiTest::StateBroadcast
{

void runAccounting (Rig& rig)
{
    auto& broadcast = rig.server.getStateBroadcast();

    check (broadcast.getClientCount() == 2,
           "the broadcaster reports both clients (got "
               + std::to_string (broadcast.getClientCount()) + ")");
    check (broadcast.getFrameCount() >= broadcast.getEventCount(),
           "every published event produced at least one client frame");

    std::cout << "state-broadcast-test: worst latency "
              << rig.worstPadLatencyMs << " ms" << std::endl;
}

} // namespace ApiTest::StateBroadcast
