#include "SandboxClientEditor.h"

#include "../PluginInstrument.h"

namespace ihub::plugins::sandbox_client {

void publishEditorState(sandbox::SharedMemory& shared,
                        PluginInstrument* plugin) noexcept {
  if (plugin == nullptr) return;

  const juce::File path{juce::String::fromUTF8(shared.editorStatePath)};

  if (path == juce::File()) return;

  try {
    const auto state = plugin->getStateInformation();

    if (state.getSize() == 0 ||
        !path.replaceWithData(state.getData(), state.getSize()))
      return;

    sandbox::storeRelaxed(shared.editorStateBytes,
                          static_cast<std::int32_t>(state.getSize()));

    const auto revision = sandbox::loadRelaxed(shared.editorStateRevision) + 1;
    sandbox::storeRelease(shared.editorStateRevision, revision);
  } catch (...) {
  }
}

void consumeEditorCapture(sandbox::SharedMemory& shared,
                          PluginInstrument* plugin) noexcept {
  if (sandbox::loadAcquire(shared.editorCaptureRequested) == 0) return;

  sandbox::storeRelaxed(shared.editorCaptureRequested, 0);
  publishEditorState(shared, plugin);
}

}  // namespace ihub::plugins::sandbox_client
