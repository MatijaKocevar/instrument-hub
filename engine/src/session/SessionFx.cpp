#include "Session.h"

namespace ihub
{

//==============================================================================
// Per-patch insert chain (T121)

bool Session::getSlotFxChain (int slot,
                              AudioEngine::FxChainSnapshot& snapshot,
                              std::string& error) const
{
    return fxChain.getSnapshot (slot, snapshot, error);
}

bool Session::addSlotFx (int slot,
                         const AudioEngine::FxInsertConfig& insert,
                         int position,
                         AudioEngine::FxChainSnapshot& snapshot,
                         std::string& error)
{
    return fxChain.add (slot, insert, position, snapshot, error, isStreamActive());
}

bool Session::replaceSlotFx (int slot,
                             const std::vector<AudioEngine::FxInsertConfig>& chain,
                             AudioEngine::FxChainSnapshot& snapshot,
                             std::string& error)
{
    return fxChain.replace (slot, chain, snapshot, error, isStreamActive());
}

bool Session::updateSlotFx (int slot,
                            int index,
                            const AudioEngine::FxInsertPatch& patch,
                            AudioEngine::FxChainSnapshot& snapshot,
                            std::string& error)
{
    return fxChain.update (slot, index, patch, snapshot, error, isStreamActive());
}

bool Session::removeSlotFx (int slot,
                            int index,
                            AudioEngine::FxChainSnapshot& snapshot,
                            std::string& error)
{
    return fxChain.remove (slot, index, snapshot, error, isStreamActive());
}

bool Session::reorderSlotFx (int slot,
                             const std::vector<int>& order,
                             AudioEngine::FxChainSnapshot& snapshot,
                             std::string& error)
{
    return fxChain.reorder (slot, order, snapshot, error, isStreamActive());
}

bool Session::clearSlotFx (int slot,
                           AudioEngine::FxChainSnapshot& snapshot,
                           std::string& error)
{
    return fxChain.clear (slot, snapshot, error, isStreamActive());
}

void Session::updateFxTransport() noexcept
{
    fxChain.updateTransport (transportControl.getFxTransport());
}

} // namespace ihub
