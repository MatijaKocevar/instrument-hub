#include "Session.h"

#include "plugins/SandboxHost.h"

#include <algorithm>

namespace ihub
{
namespace
{
bool sameIdentity (const session::PluginStateIdentity& left,
                   const session::PluginStateIdentity& right)
{
    return left.pluginPath == right.pluginPath
        && left.pluginId == right.pluginId
        && left.pluginName == right.pluginName;
}
}

void Session::registerSandboxSlot (int slot,
                                   plugins::SandboxHost& host,
                                   const session::PluginStateIdentity& identity)
{
    const std::lock_guard<std::mutex> lock (sandboxMutex);

    sandboxSlots.erase (std::remove_if (sandboxSlots.begin(),
                                        sandboxSlots.end(),
                                        [&host] (const SandboxSlot& entry)
                                        {
                                            return entry.host == &host;
                                        }),
                        sandboxSlots.end());

    sandboxSlots.push_back ({ slot, &host, identity });
}

void Session::unregisterSandboxSlot (plugins::SandboxHost& host)
{
    SandboxSlot removed;

    {
        const std::lock_guard<std::mutex> lock (sandboxMutex);

        const auto found = std::find_if (sandboxSlots.begin(),
                                         sandboxSlots.end(),
                                         [&host] (const SandboxSlot& entry)
                                         {
                                             return entry.host == &host;
                                         });

        if (found == sandboxSlots.end())
            return;

        removed = *found;
        sandboxSlots.erase (found);
    }

    if (shuttingDown.load (std::memory_order_acquire))
        return;

    session::PluginStateIdentity current;

    if (! pluginIdentityForSlot (removed.slot, current)
        || ! sameIdentity (current, removed.identity))
        return;

    if (pluginState.getInstrument (removed.slot) != nullptr)
    {
        (void) capturePluginState (removed.slot);
        return;
    }

    const auto state = host.getStateInformation();

    if (state.getSize() > 0)
        (void) pluginState.storeState (removed.slot, removed.identity, state);
}

plugins::SandboxHost* Session::sandboxForSlot (int slot) const noexcept
{
    const std::lock_guard<std::mutex> lock (sandboxMutex);

    for (auto entry = sandboxSlots.rbegin(); entry != sandboxSlots.rend(); ++entry)
        if (entry->slot == slot)
            return entry->host;

    return nullptr;
}

plugins::SandboxHost* Session::getSandboxHost (int slot) const noexcept
{
    return sandboxForSlot (slot);
}

int Session::getSandboxProcessId (int slot) const noexcept
{
    auto* host = sandboxForSlot (slot);
    return host != nullptr ? host->getChildProcessId() : 0;
}

void Session::setSlotError (int slot, const juce::String& error)
{
    const std::lock_guard<std::mutex> lock (slotErrorMutex);

    if (slot >= 0 && slot < static_cast<int> (slotErrors.size()))
        slotErrors[static_cast<std::size_t> (slot)] = error;
}

void Session::clearSlotError (int slot)
{
    setSlotError (slot, {});
}

juce::String Session::getSlotError (int slot) const
{
    const std::lock_guard<std::mutex> lock (slotErrorMutex);

    if (slot < 0 || slot >= static_cast<int> (slotErrors.size()))
        return {};

    return slotErrors[static_cast<std::size_t> (slot)];
}

bool Session::isSlotPluginFailed (int slot) const noexcept
{
    auto* host = sandboxForSlot (slot);
    return host != nullptr && host->isFailed();
}

}
