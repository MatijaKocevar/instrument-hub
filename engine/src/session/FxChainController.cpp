#include "FxChainController.h"

#include "Session.h"

#include <juce_events/juce_events.h>

#include <algorithm>

namespace ihub::session {

FxChainController::FxChainController() = default;

FxChainController::~FxChainController() = default;

void FxChainController::setEngine(
    AudioEngine::AudioEngine* engineToUse) noexcept {
  engine = engineToUse;
}

void FxChainController::setFormat(double newSampleRate, int newMaximumBlockSize,
                                  int newNumChannels) noexcept {
  sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
  maximumBlockSize = std::max(1, newMaximumBlockSize);
  numChannels = std::max(1, newNumChannels);
}

void FxChainController::build(const std::vector<SessionSlotConfig>& slotConfigs,
                              juce::AudioPluginFormatManager& formatManager,
                              const AudioEngine::FxTransport& transport) {
  reset();
  formats = &formatManager;

  if (engine == nullptr) return;

  const int slotCount = static_cast<int>(slotConfigs.size());
  racks.reserve(static_cast<std::size_t>(slotCount));

  for (int index = 0; index < slotCount; ++index) {
    auto rack = std::make_unique<AudioEngine::FxRack>(formats);
    const auto& slotConfig = slotConfigs[static_cast<std::size_t>(index)];

    for (const auto& fx : slotConfig.fxChain) {
      std::string fxError;

      if (!rack->add(fx, -1, sampleRate, maximumBlockSize, numChannels,
                     fxError))
        juce::Logger::writeToLog("session: slot " + juce::String(index) +
                                 " insert skipped: " + juce::String(fxError));
    }

    rack->setTransport(transport);
    rack->rebuild(engine->getSlot(index).getFxChain(), sampleRate,
                  maximumBlockSize, numChannels);
    racks.push_back(std::move(rack));
  }
}

void FxChainController::reset() noexcept { racks.clear(); }

bool FxChainController::getSnapshot(int slot,
                                    AudioEngine::FxChainSnapshot& snapshot,
                                    std::string& error) const {
  const std::lock_guard<std::mutex> lock(mutex);

  if (!makeSnapshot(slot, snapshot)) {
    error = "patch slot is out of range";
    return false;
  }

  return true;
}

bool FxChainController::getTunerReading(int slot, int index,
                                        AudioEngine::FxTunerReading& reading,
                                        std::string& error) const {
  const std::lock_guard<std::mutex> lock(mutex);

  if (slot < 0 || slot >= static_cast<int>(racks.size())) {
    error = "patch slot is out of range";
    return false;
  }

  if (!racks[static_cast<std::size_t>(slot)]->getTunerReading(index, reading)) {
    error = "the insert is not a tuner";
    return false;
  }

  return true;
}

bool FxChainController::add(int slot, const AudioEngine::FxInsertConfig& insert,
                            int position,
                            AudioEngine::FxChainSnapshot& snapshot,
                            std::string& error, bool streamActive) {
  const std::lock_guard<std::mutex> lock(mutex);

  if (!beginEdit(slot, streamActive, error)) return false;

  const bool added = racks[static_cast<std::size_t>(slot)]->add(
      insert, position, sampleRate, maximumBlockSize, numChannels, error);

  if (added) rebuild(slot);

  endEdit(slot);

  return added && makeSnapshot(slot, snapshot);
}

bool FxChainController::replace(
    int slot, const std::vector<AudioEngine::FxInsertConfig>& chain,
    AudioEngine::FxChainSnapshot& snapshot, std::string& error,
    bool streamActive) {
  const std::lock_guard<std::mutex> lock(mutex);

  if (!beginEdit(slot, streamActive, error)) return false;

  const bool replaced = racks[static_cast<std::size_t>(slot)]->replace(
      chain, sampleRate, maximumBlockSize, numChannels, error);

  if (replaced) rebuild(slot);

  endEdit(slot);

  return replaced && makeSnapshot(slot, snapshot);
}

bool FxChainController::update(int slot, int index,
                               const AudioEngine::FxInsertPatch& patch,
                               AudioEngine::FxChainSnapshot& snapshot,
                               std::string& error, bool streamActive) {
  const std::lock_guard<std::mutex> lock(mutex);

  if (slot < 0 || slot >= static_cast<int>(racks.size())) {
    error = "patch slot is out of range";
    return false;
  }

  const bool reorders = patch.position.has_value();

  if (reorders && !beginEdit(slot, streamActive, error)) return false;

  const bool updated = racks[static_cast<std::size_t>(slot)]->update(
      index, patch, sampleRate, maximumBlockSize, numChannels, error);

  if (updated && reorders) rebuild(slot);

  if (reorders) endEdit(slot);

  return updated && makeSnapshot(slot, snapshot);
}

bool FxChainController::remove(int slot, int index,
                               AudioEngine::FxChainSnapshot& snapshot,
                               std::string& error, bool streamActive) {
  const std::lock_guard<std::mutex> lock(mutex);

  if (!beginEdit(slot, streamActive, error)) return false;

  const bool removed =
      racks[static_cast<std::size_t>(slot)]->remove(index, error);

  if (removed) rebuild(slot);

  endEdit(slot);

  return removed && makeSnapshot(slot, snapshot);
}

bool FxChainController::reorder(int slot, const std::vector<int>& order,
                                AudioEngine::FxChainSnapshot& snapshot,
                                std::string& error, bool streamActive) {
  const std::lock_guard<std::mutex> lock(mutex);

  if (!beginEdit(slot, streamActive, error)) return false;

  const bool reordered =
      racks[static_cast<std::size_t>(slot)]->reorder(order, error);

  if (reordered) rebuild(slot);

  endEdit(slot);

  return reordered && makeSnapshot(slot, snapshot);
}

bool FxChainController::clear(int slot, AudioEngine::FxChainSnapshot& snapshot,
                              std::string& error, bool streamActive) {
  const std::lock_guard<std::mutex> lock(mutex);

  if (!beginEdit(slot, streamActive, error)) return false;

  racks[static_cast<std::size_t>(slot)]->clear();
  rebuild(slot);
  endEdit(slot);

  return makeSnapshot(slot, snapshot);
}

void FxChainController::updateTransport(
    const AudioEngine::FxTransport& transport) noexcept {
  const std::lock_guard<std::mutex> lock(mutex);

  for (auto& rack : racks)
    if (rack != nullptr) rack->setTransport(transport);
}

bool FxChainController::beginEdit(int slot, bool streamActive,
                                  std::string& error) {
  if (engine == nullptr || !engine->isPrepared() || slot < 0 ||
      slot >= static_cast<int>(racks.size())) {
    error = "patch slot is out of range";
    return false;
  }

  auto& audioSlot = engine->getSlot(slot);
  audioSlot.setFxChainEditing(true);

  if (streamActive) {
    const auto before = renderEpoch.load(std::memory_order_acquire);
    const double deadline = juce::Time::getMillisecondCounterHiRes() + 250.0;

    while (renderEpoch.load(std::memory_order_acquire) == before) {
      if (juce::Time::getMillisecondCounterHiRes() >= deadline) {
        audioSlot.setFxChainEditing(false);
        error = "the audio thread did not reach a block boundary; try again";
        return false;
      }

      juce::Thread::sleep(1);
    }
  }

  return true;
}

void FxChainController::endEdit(int slot) noexcept {
  if (engine != nullptr && slot >= 0 && slot < engine->getNumSlots())
    engine->getSlot(slot).setFxChainEditing(false);
}

void FxChainController::rebuild(int slot) {
  if (engine == nullptr || slot < 0 || slot >= static_cast<int>(racks.size()))
    return;

  auto& audioSlot = engine->getSlot(slot);
  racks[static_cast<std::size_t>(slot)]->rebuild(
      audioSlot.getFxChain(), sampleRate, maximumBlockSize, numChannels);
  audioSlot.getFxChain().refreshLatency();
}

bool FxChainController::makeSnapshot(
    int slot, AudioEngine::FxChainSnapshot& snapshot) const {
  if (engine == nullptr || slot < 0 || slot >= static_cast<int>(racks.size()))
    return false;

  const auto& audioSlot = engine->getSlot(slot);
  const auto& rack = *racks[static_cast<std::size_t>(slot)];

  const int total = audioSlot.latencySamples();
  const int fxLatency = rack.latencySamples();

  snapshot = {};
  snapshot.slot = slot;
  snapshot.fxLatencySamples = fxLatency;
  snapshot.latencySamples = total;
  snapshot.instrumentLatencySamples = std::max(0, total - fxLatency);
  snapshot.latencyMs =
      sampleRate > 0.0 ? static_cast<double>(total) * 1000.0 / sampleRate : 0.0;
  snapshot.inserts = rack.snapshots();
  return true;
}

}  // namespace ihub::session
