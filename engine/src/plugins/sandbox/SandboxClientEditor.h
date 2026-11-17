#pragma once

#include "../SandboxClient.h"

namespace ihub::plugins {
class PluginInstrument;
}

namespace ihub::plugins::sandbox_client {

void publishEditorState(sandbox::SharedMemory& shared,
                        PluginInstrument* plugin) noexcept;
void consumeEditorCapture(sandbox::SharedMemory& shared,
                          PluginInstrument* plugin) noexcept;

}  // namespace ihub::plugins::sandbox_client
