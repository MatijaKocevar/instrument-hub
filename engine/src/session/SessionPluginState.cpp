#include "Session.h"

#include "plugins/PluginInstrument.h"

namespace ihub
{

//==============================================================================
// Per-patch plugin state (T130)

void Session::setPluginStateDirectory (const juce::File& directory)
{
    pluginState.setDirectory (directory);
}

bool Session::loadPluginStates()
{
    return pluginState.loadAll();
}

bool Session::savePluginStates()
{
    return pluginState.saveAll();
}

bool Session::capturePluginState (int slot)
{
    session::PluginStateIdentity identity;

    if (! pluginIdentityForSlot (slot, identity))
        return false;

    return pluginState.capture (slot, identity);
}

bool Session::hasPluginState (int slot) const noexcept
{
    return pluginState.hasState (slot);
}

juce::int64 Session::getPluginStateSize (int slot) const noexcept
{
    return pluginState.getStateSize (slot);
}

plugins::PluginInstrument* Session::getPluginInstrument (int slot) const noexcept
{
    return pluginState.getInstrument (slot);
}

void Session::capturePluginStateForInstance (AudioEngine::SlotSource& instance)
{
    auto* instrument = dynamic_cast<plugins::PluginInstrument*> (&instance);

    if (instrument == nullptr)
        return;

    const int slot = pluginState.findSlotForInstrument (*instrument);

    if (slot >= 0)
        (void) capturePluginState (slot);
}

bool Session::pluginIdentityForSlot (int slot,
                                     session::PluginStateIdentity& identity) const
{
    const std::lock_guard<std::mutex> sourceLock (sourceMutex);

    if (slot < 0 || slot >= static_cast<int> (sourceConfigs.size())
        || sourceConfigs[static_cast<std::size_t> (slot)].kind
               != SessionSlotKind::plugin)
        return false;

    const auto& source = sourceConfigs[static_cast<std::size_t> (slot)];
    identity.pluginPath = source.path;
    identity.pluginId = juce::String (source.pluginId);
    identity.pluginName = juce::String (source.pluginName);
    return true;
}

} // namespace ihub
