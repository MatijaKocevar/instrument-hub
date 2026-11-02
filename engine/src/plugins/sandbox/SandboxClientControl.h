#pragma once

#include "../SandboxClient.h"

#include <cstdint>

namespace ihub::plugins
{
class PluginInstrument;

namespace sandbox_client
{

void handleControl (sandbox::SharedMemory& shared,
                    PluginInstrument* plugin,
                    std::uint32_t& lastSequence);

}
}
