#include "audio/FxRack.h"

#include <cstddef>

namespace AudioEngine {

FxInsertSnapshot FxRack::getInsert(int index) const {
  FxInsertSnapshot snapshot;

  if (index < 0 || index >= size()) return snapshot;

  const auto& insert = inserts[static_cast<std::size_t>(index)];

  snapshot.index = index;
  snapshot.type = insert.type;
  snapshot.kind = insert.type == FxInsertType::vst3 ? "vst3" : "builtin";
  snapshot.id = insert.id;
  snapshot.name = insert.name;

  if (insert.effect != nullptr) {
    snapshot.bypassed = insert.effect->isBypassed();
    snapshot.wet = insert.effect->getWet();
    snapshot.latencySamples = insert.effect->getLatencySamples();
    snapshot.parameters = readParameters(*insert.effect);
  }

  return snapshot;
}

std::vector<FxInsertSnapshot> FxRack::snapshots() const {
  std::vector<FxInsertSnapshot> result;
  result.reserve(inserts.size());

  for (int index = 0; index < size(); ++index)
    result.push_back(getInsert(index));

  return result;
}

}  // namespace AudioEngine
