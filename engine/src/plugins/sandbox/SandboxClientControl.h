#pragma once

#include "../SandboxClient.h"

#include <cstdint>
#include <functional>

namespace ihub::plugins
{
class PluginInstrument;

namespace sandbox_client
{

using EditorControl = std::function<bool (bool open, juce::String& error)>;

void handleControl (sandbox::SharedMemory& shared,
                    PluginInstrument* plugin,
                    std::uint32_t& lastSequence,
                    EditorControl editorControl = {});

}
}
