#include "api/StateStore.h"

#include <utility>

namespace Api
{

//==============================================================================
// Per-patch insert chain (T121)

void StateStore::setFxHandlers (FxChainProvider provider, FxCommandHandler handler)
{
    const std::lock_guard<std::mutex> lock (mutex);
    fxProvider = std::move (provider);
    fxHandler = std::move (handler);
}

bool StateStore::getFxChain (int slot, FxChainState& chain, std::string& error) const
{
    FxChainProvider provider;

    {
        const std::lock_guard<std::mutex> lock (mutex);

        if (slot < 0 || slot >= numSlots)
        {
            error = "patch slot is out of range";
            return false;
        }

        provider = fxProvider;
    }

    if (provider == nullptr)
    {
        error = "no audio session is attached";
        return false;
    }

    try
    {
        return provider (slot, chain, error);
    }
    catch (...)
    {
        error = "the FX chain reader failed";
        return false;
    }
}

bool StateStore::applyFxCommand (int slot,
                                 const FxCommand& command,
                                 FxChainState& chain,
                                 std::string& error)
{
    FxCommandHandler handler;
    ChangeHandler changed;

    {
        const std::lock_guard<std::mutex> lock (mutex);

        if (slot < 0 || slot >= numSlots)
        {
            error = "patch slot is out of range";
            return false;
        }

        handler = fxHandler;
        changed = changeHandler;
    }

    if (handler == nullptr)
    {
        error = "no audio session is attached";
        return false;
    }

    bool applied = false;

    try
    {
        applied = handler (slot, command, chain, error);
    }
    catch (...)
    {
        error = "the FX chain edit failed";
        return false;
    }

    if (applied && changed != nullptr)
        changed ("patch", slot);

    return applied;
}

} // namespace Api
