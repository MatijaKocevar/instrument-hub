#include "api/StateStore.h"

#include <algorithm>
#include <string>

namespace Api {

bool StateStore::assignTrackFx(int index, int hot, const std::string& insertId,
                               std::string& error) {
  int slot = -1;

  {
    const std::lock_guard<std::mutex> lock(mutex);

    if (index < 0 || index >= static_cast<int>(tracks.size())) {
      error = "track index is out of range";
      return false;
    }

    if (hot < 0 || hot >= 2) {
      error = "hot fx index is out of range";
      return false;
    }

    slot = tracks[static_cast<std::size_t>(index)].slot;

    if (slot < 0 || slot >= numSlots) {
      error = "track slot is out of range";
      return false;
    }
  }

  if (!insertId.empty()) {
    FxChainState chain;

    if (!getFxChain(slot, chain, error)) return false;

    const auto insert =
        std::find_if(chain.inserts.begin(), chain.inserts.end(),
                     [&insertId](const FxInsertState& candidate) {
                       return candidate.id == insertId;
                     });

    if (insert == chain.inserts.end()) {
      error = "hot fx insert \"" + insertId + "\" is not in the chain";
      return false;
    }
  }

  {
    const std::lock_guard<std::mutex> lock(mutex);

    auto& track = tracks[static_cast<std::size_t>(index)];
    track.hotFx.resize(2);

    if (insertId.empty()) {
      track.hotFx[static_cast<std::size_t>(hot)].clear();
    } else {
      for (auto position = std::size_t{0}; position < track.hotFx.size();
           ++position) {
        if (position != static_cast<std::size_t>(hot) &&
            track.hotFx[position] == insertId)
          track.hotFx[position].clear();
      }

      track.hotFx[static_cast<std::size_t>(hot)] = insertId;
    }
  }

  notifyEdited("track", slot);
  notifyChanged("track", index);
  return true;
}

}  // namespace Api
