#include "Mapping.h"

#include <utility>

namespace Controller {

Mapping::Mapping() = default;

Mapping::~Mapping() { stopWatching(); }

void Mapping::setListeners(Listeners newListeners) {
  const std::lock_guard<std::mutex> lock(mutex);
  listeners = std::move(newListeners);
}

std::filesystem::path Mapping::getPath() const {
  const std::lock_guard<std::mutex> lock(mutex);
  return path;
}

std::shared_ptr<const SetMapping> Mapping::getMapping() const {
  const std::lock_guard<std::mutex> lock(mutex);
  return current;
}

std::uint64_t Mapping::getRevision() const noexcept {
  return revision.load(std::memory_order_acquire);
}

MappingError Mapping::getLastError() const {
  const std::lock_guard<std::mutex> lock(mutex);
  return lastError;
}

std::optional<PadBinding> Mapping::getPadBinding(int pad) const {
  const std::lock_guard<std::mutex> lock(mutex);

  if (current == nullptr) return std::nullopt;

  if (const auto* binding = current->padBinding(pad)) return *binding;

  return std::nullopt;
}

std::optional<EncoderBinding> Mapping::getEncoderBinding(int encoder) const {
  const std::lock_guard<std::mutex> lock(mutex);

  if (current == nullptr) return std::nullopt;

  if (const auto* binding = current->encoderBinding(encoder)) return *binding;

  return std::nullopt;
}

std::optional<MacroBankSpec> Mapping::getBank(int bank) const {
  const std::lock_guard<std::mutex> lock(mutex);

  if (current == nullptr) return std::nullopt;

  if (const auto* page = current->bankBinding(bank)) return *page;

  return std::nullopt;
}

std::optional<SceneSpec> Mapping::getScene(int scene) const {
  const std::lock_guard<std::mutex> lock(mutex);

  if (current == nullptr) return std::nullopt;

  if (const auto* selection = current->sceneBinding(scene)) return *selection;

  return std::nullopt;
}

int Mapping::getActivePad() const noexcept {
  return activePad.load(std::memory_order_acquire);
}

int Mapping::getActiveBank() const noexcept {
  return activeBank.load(std::memory_order_acquire);
}

int Mapping::getActiveScene() const noexcept {
  return activeScene.load(std::memory_order_acquire);
}

int Mapping::getArmedSlot() const noexcept {
  const std::lock_guard<std::mutex> lock(mutex);
  const int pad = activePad.load(std::memory_order_relaxed);

  if (current != nullptr && pad >= 0)
    if (const auto* binding = current->padBinding(pad)) return binding->slot;

  return -1;
}

}  // namespace Controller
