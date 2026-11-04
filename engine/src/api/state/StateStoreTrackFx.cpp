#include "api/StateStore.h"

#include <algorithm>
#include <string>

namespace Api
{

bool StateStore::toggleTrackFx (int index, int hot, std::string& error)
{
    int slot = -1;
    std::string fxId;
    FxCommandHandler handler;

    {
        const std::lock_guard<std::mutex> lock (mutex);

        if (index < 0 || index >= static_cast<int> (tracks.size()))
        {
            error = "track index is out of range";
            return false;
        }

        const auto& track = tracks[static_cast<std::size_t> (index)];

        if (hot < 0 || hot >= static_cast<int> (track.hotFx.size()))
        {
            error = "hot fx index is out of range";
            return false;
        }

        fxId = track.hotFx[static_cast<std::size_t> (hot)];
        slot = track.slot;
        handler = fxHandler;
    }

    if (fxId.empty())
    {
        error = "no hot fx is assigned to that index";
        return false;
    }

    if (handler == nullptr)
    {
        error = "no audio session is attached";
        return false;
    }

    FxChainState chain;

    if (! getFxChain (slot, chain, error))
        return false;

    const auto insert = std::find_if (chain.inserts.begin(), chain.inserts.end(),
                                      [&fxId] (const FxInsertState& candidate)
                                      {
                                          return candidate.id == fxId;
                                      });

    if (insert == chain.inserts.end())
    {
        error = "hot fx insert \"" + fxId + "\" is not in the chain";
        return false;
    }

    FxCommand command;
    command.kind = FxCommandKind::update;
    command.index = insert->index;
    command.update.bypassed = ! insert->bypassed;

    bool applied = false;

    try
    {
        applied = handler (slot, command, chain, error);
    }
    catch (...)
    {
        error = "the FX chain toggle failed";
        return false;
    }

    if (! applied)
        return false;

    notifyEdited ("track", slot);
    notifyChanged ("track", index);
    return true;
}

}
