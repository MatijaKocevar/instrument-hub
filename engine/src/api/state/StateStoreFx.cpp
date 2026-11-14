#include "api/StateStore.h"

#include <algorithm>
#include <utility>

namespace Api
{

    void StateStore::setFxHandlers (FxChainProvider provider, FxCommandHandler handler)
    {
        const std::lock_guard<std::mutex> lock (mutex);
        fxProvider = std::move (provider);
        fxHandler = std::move (handler);
    }

    void StateStore::setFxTunerProvider (FxTunerProvider provider)
    {
        const std::lock_guard<std::mutex> lock (mutex);
        fxTunerProvider = std::move (provider);
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

    bool StateStore::getFxTuner (int slot, int index, FxTunerState& tuner, std::string& error) const
    {
        FxTunerProvider provider;

        {
            const std::lock_guard<std::mutex> lock (mutex);

            if (slot < 0 || slot >= numSlots)
            {
                error = "patch slot is out of range";
                return false;
            }

            provider = fxTunerProvider;
        }

        if (provider == nullptr)
        {
            error = "no audio session is attached";
            return false;
        }

        try
        {
            return provider (slot, index, tuner, error);
        }
        catch (...)
        {
            error = "the tuner reader failed";
            return false;
        }
    }

    bool StateStore::applyFxCommand (int slot,
                                     const FxCommand& command,
                                     FxChainState& chain,
                                     std::string& error)
    {
        FxCommandHandler handler;

        {
            const std::lock_guard<std::mutex> lock (mutex);

            if (slot < 0 || slot >= numSlots)
            {
                error = "patch slot is out of range";
                return false;
            }

            handler = fxHandler;
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

        if (applied)
        {
            std::vector<int> changedTracks;

            if (command.kind == FxCommandKind::remove || command.kind == FxCommandKind::clear ||
                command.kind == FxCommandKind::replace)
            {
                const auto hasInsert = [&chain] (const std::string& id)
                {
                    return std::any_of (chain.inserts.begin (),
                                        chain.inserts.end (),
                                        [&id] (const FxInsertState& insert)
                                        { return insert.id == id; });
                };

                const std::lock_guard<std::mutex> lock (mutex);

                for (std::size_t index = 0; index < tracks.size (); ++index)
                {
                    auto& track = tracks[index];

                    if (track.slot != slot)
                        continue;

                    bool changed = false;

                    for (auto& hot : track.hotFx)
                    {
                        if (hot.empty () || hasInsert (hot))
                            continue;

                        hot.clear ();
                        changed = true;
                    }

                    if (changed)
                        changedTracks.push_back (static_cast<int> (index));
                }
            }

            notifyEdited ("patch", slot);
            notifyChanged ("patch", slot);

            for (const int index : changedTracks)
            {
                notifyEdited ("track", index);
                notifyChanged ("track", index);
            }
        }

        return applied;
    }

}
