#include "api/StateStore.h"

#include <cctype>
#include <string>

namespace Api {

namespace {
std::string trimmed(const std::string& text) {
  std::size_t first = 0;

  while (first < text.size() &&
         std::isspace(static_cast<unsigned char>(text[first])) != 0)
    ++first;

  std::size_t last = text.size();

  while (last > first &&
         std::isspace(static_cast<unsigned char>(text[last - 1])) != 0)
    --last;

  return text.substr(first, last - first);
}
}  // namespace

bool StateStore::renameTrack(int index, const std::string& name,
                             std::string& error) {
  const auto next = trimmed(name);

  if (next.empty()) {
    error = "track name must not be empty";
    return false;
  }

  if (next.size() > 80) {
    error = "track name must be at most 80 characters";
    return false;
  }

  int slot = -1;

  {
    const std::lock_guard<std::mutex> lock(mutex);

    if (index < 0 || index >= static_cast<int>(tracks.size())) {
      error = "track index is out of range";
      return false;
    }

    auto& track = tracks[static_cast<std::size_t>(index)];

    if (track.slot < 0 || track.slot >= numSlots) {
      error = "track slot is out of range";
      return false;
    }

    if (track.name == next) return true;

    track.name = next;
    slot = track.slot;
    slotNames[static_cast<std::size_t>(slot)] = next;
    slotFamilyNames[static_cast<std::size_t>(slot)] = next;
    slotPatchNames[static_cast<std::size_t>(slot)] = next;

    for (auto& family : families) {
      if (family.id != track.id) continue;

      family.name = next;

      for (auto& patch : family.patches) {
        if (patch.slot != slot) continue;

        patch.name = next;
        patch.familyName = next;
      }
    }
  }

  notifyEdited("track", slot);
  notifyChanged("track", index);
  return true;
}

}  // namespace Api
