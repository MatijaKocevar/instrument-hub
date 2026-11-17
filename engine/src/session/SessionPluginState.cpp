#include "Session.h"

#include "plugins/PluginInstrument.h"
#include "plugins/SandboxHost.h"
#include "plugins/SandboxSlotControl.h"

namespace ihub {

void Session::setPluginStateDirectory(const juce::File& directory) {
    pluginState.setDirectory(directory);
}

bool Session::loadPluginStates() { return pluginState.loadAll(); }

bool Session::savePluginStates() { return pluginState.saveAll(); }

bool Session::capturePluginState(int slot) {
    session::PluginStateIdentity identity;

    if (!pluginIdentityForSlot(slot, identity)) return false;

    if (pluginState.getInstrument(slot) != nullptr) return pluginState.capture(slot, identity);

    auto* sandbox = getSandboxSlotControl(slot);

    if (sandbox == nullptr) return false;

    (void)sandbox->flushPublishedState();

    auto state = sandbox->isRunning() && !sandbox->isFailed() ? sandbox->getStateInformation()
                                                              : juce::MemoryBlock();

    if (state.getSize() == 0) state = sandbox->getLastCommittedState();

    if (state.getSize() == 0) return false;

    return pluginState.storeState(slot, identity, state);
}

bool Session::copyPluginStateData(int slot, juce::MemoryBlock& data) const {
    session::PluginStateIdentity identity;

    if (!pluginIdentityForSlot(slot, identity)) return false;

    data = pluginState.readState(slot, identity);
    return data.getSize() > 0;
}

bool Session::hasPluginState(int slot) const noexcept { return pluginState.hasState(slot); }

juce::int64 Session::getPluginStateSize(int slot) const noexcept {
    return pluginState.getStateSize(slot);
}

plugins::PluginInstrument* Session::getPluginInstrument(int slot) {
    if (auto* instrument = pluginState.getInstrument(slot); instrument != nullptr)
        return instrument;

    std::string error;

    if (!ensurePluginMirror(slot, error)) return nullptr;

    return pluginState.getInstrument(slot);
}

void Session::capturePluginStateForInstance(AudioEngine::SlotSource& instance) {
    auto* instrument = dynamic_cast<plugins::PluginInstrument*>(&instance);

    if (instrument == nullptr) return;

    const int slot = pluginState.findSlotForInstrument(*instrument);

    if (slot >= 0) (void)capturePluginState(slot);
}

bool Session::pluginIdentityForSlot(int slot, session::PluginStateIdentity& identity) const {
    const std::lock_guard<std::mutex> sourceLock(sourceMutex);

    if (slot < 0 || slot >= static_cast<int>(sourceConfigs.size()) ||
        sourceConfigs[static_cast<std::size_t>(slot)].kind != SessionSlotKind::plugin)
        return false;

    const auto& source = sourceConfigs[static_cast<std::size_t>(slot)];
    identity.pluginPath = source.path;
    identity.pluginId = juce::String(source.pluginId);
    identity.pluginName = juce::String(source.pluginName);
    return true;
}

}  // namespace ihub
