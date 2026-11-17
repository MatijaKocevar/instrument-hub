#include "PluginStateStore.h"

#include "PluginStateFile.h"

#include "plugins/PluginInstrument.h"

#include <juce_core/juce_core.h>

#include <utility>

namespace ihub::session {

PluginStateStore::PluginStateStore(int slotCapacity)
    : states(static_cast<std::size_t>(juce::jmax(0, slotCapacity))),
      instruments(static_cast<std::size_t>(juce::jmax(0, slotCapacity)),
                  nullptr) {}

void PluginStateStore::setDirectory(const juce::File& newDirectory) {
  {
    const std::lock_guard<std::mutex> lock(mutex);
    directory = newDirectory;
  }

  (void)loadAll();
}

juce::File PluginStateStore::getDirectory() const {
  const std::lock_guard<std::mutex> lock(mutex);
  return directory;
}

bool PluginStateStore::loadAll() {
  const std::lock_guard<std::mutex> lock(mutex);

  for (auto& state : states) state = {};

  if (directory == juce::File()) return false;

  if (!directory.isDirectory()) return true;

  bool allRead = true;

  for (int slot = 0; slot < static_cast<int>(states.size()); ++slot) {
    const auto file = pluginStateFileFor(directory, slot);

    if (!file.existsAsFile()) continue;

    if (!readFileLocked(slot, file)) {
      allRead = false;
      juce::Logger::writeToLog("session: unreadable plugin state " +
                               file.getFullPathName());
    }
  }

  return allRead;
}

bool PluginStateStore::saveAll() {
  juce::File currentDirectory;

  {
    const std::lock_guard<std::mutex> lock(mutex);
    currentDirectory = directory;
  }

  if (currentDirectory == juce::File()) return false;

  bool saved = true;

  for (int slot = 0; slot < static_cast<int>(states.size()); ++slot) {
    bool hasData = false;

    {
      const std::lock_guard<std::mutex> lock(mutex);
      hasData = states[static_cast<std::size_t>(slot)].data.getSize() > 0;
    }

    if (hasData && !write(slot)) saved = false;
  }

  return saved;
}

bool PluginStateStore::capture(int slot, const PluginStateIdentity& identity) {
  plugins::PluginInstrument* instrument = getInstrument(slot);

  if (instrument == nullptr) return false;

  const auto data = instrument->getStateInformation();

  if (data.getSize() == 0) return false;

  return storeState(slot, identity, data);
}

void PluginStateStore::apply(int slot, plugins::PluginInstrument& instrument,
                             const PluginStateIdentity& identity, bool force) {
  juce::MemoryBlock data;
  std::uint64_t revision = 0;

  {
    const std::lock_guard<std::mutex> lock(mutex);

    if (slot < 0 || slot >= static_cast<int>(states.size())) return;

    auto& state = states[static_cast<std::size_t>(slot)];

    if (state.data.getSize() == 0) return;

    PluginStateFileData stored;
    stored.pluginPath = state.pluginPath;
    stored.pluginId = state.pluginId;

    if (!pluginStateMatches(stored, identity.pluginPath, identity.pluginId)) {
      state.appliedRevision = state.revision;
      return;
    }

    if (!force && state.appliedRevision == state.revision &&
        state.appliedRevision != 0)
      return;

    data = state.data;
    revision = state.revision;
  }

  const bool accepted = instrument.setStateInformation(data);

  if (!accepted) {
    juce::Logger::writeToLog("session: slot " + juce::String(slot) +
                             " rejected stored plugin state \"" +
                             (identity.pluginName.isEmpty()
                                  ? juce::String("plugin")
                                  : identity.pluginName) +
                             "\"");
  }

  const std::lock_guard<std::mutex> lock(mutex);

  if (slot >= 0 && slot < static_cast<int>(states.size()) &&
      states[static_cast<std::size_t>(slot)].revision == revision)
    states[static_cast<std::size_t>(slot)].appliedRevision = revision;
}

void PluginStateStore::applyToRegistered(int slot,
                                         const PluginStateIdentity& identity) {
  plugins::PluginInstrument* instrument = getInstrument(slot);

  if (instrument != nullptr) apply(slot, *instrument, identity);
}

void PluginStateStore::clear(int slot) {
  if (slot < 0 || slot >= static_cast<int>(states.size())) return;

  juce::File currentDirectory;

  {
    const std::lock_guard<std::mutex> lock(mutex);

    currentDirectory = directory;
    states[static_cast<std::size_t>(slot)] = {};
  }

  if (currentDirectory != juce::File())
    pluginStateFileFor(currentDirectory, slot).deleteFile();
}

bool PluginStateStore::hasState(int slot) const noexcept {
  if (slot < 0 || slot >= static_cast<int>(states.size())) return false;

  const std::lock_guard<std::mutex> lock(mutex);
  return states[static_cast<std::size_t>(slot)].data.getSize() > 0;
}

juce::int64 PluginStateStore::getStateSize(int slot) const noexcept {
  if (slot < 0 || slot >= static_cast<int>(states.size())) return 0;

  const std::lock_guard<std::mutex> lock(mutex);
  return static_cast<juce::int64>(
      states[static_cast<std::size_t>(slot)].data.getSize());
}

void PluginStateStore::registerInstrument(
    int slot, plugins::PluginInstrument& instrument) noexcept {
  if (slot < 0 || slot >= static_cast<int>(instruments.size())) return;

  const std::lock_guard<std::mutex> lock(mutex);
  instruments[static_cast<std::size_t>(slot)] = &instrument;
}

void PluginStateStore::unregisterInstrument(
    plugins::PluginInstrument& instrument) noexcept {
  const std::lock_guard<std::mutex> lock(mutex);

  for (auto& entry : instruments)
    if (entry == &instrument) entry = nullptr;
}

plugins::PluginInstrument* PluginStateStore::getInstrument(
    int slot) const noexcept {
  if (slot < 0 || slot >= static_cast<int>(instruments.size())) return nullptr;

  const std::lock_guard<std::mutex> lock(mutex);
  return instruments[static_cast<std::size_t>(slot)];
}

int PluginStateStore::findSlotForInstrument(
    plugins::PluginInstrument& instrument) const noexcept {
  const std::lock_guard<std::mutex> lock(mutex);

  for (int slot = 0; slot < static_cast<int>(instruments.size()); ++slot)
    if (instruments[static_cast<std::size_t>(slot)] == &instrument) return slot;

  return -1;
}

bool PluginStateStore::write(int slot) {
  juce::MemoryBlock data;
  juce::String pluginPath;
  juce::String pluginId;
  juce::String pluginName;
  std::uint64_t revision = 0;
  juce::File currentDirectory;

  {
    const std::lock_guard<std::mutex> lock(mutex);

    currentDirectory = directory;

    if (slot < 0 || slot >= static_cast<int>(states.size())) return false;

    const auto& state = states[static_cast<std::size_t>(slot)];

    if (state.data.getSize() == 0) return false;

    data = state.data;
    pluginPath = state.pluginPath;
    pluginId = state.pluginId;
    pluginName = state.pluginName;
    revision = state.revision;
  }

  if (currentDirectory == juce::File()) return true;

  return writePluginStateFile(pluginStateFileFor(currentDirectory, slot), slot,
                              data, pluginPath, pluginId, pluginName, revision);
}

bool PluginStateStore::readFileLocked(int slot, const juce::File& file) {
  PluginStateFileData stored;

  if (!readPluginStateFileData(file, slot, stored)) return false;

  auto& state = states[static_cast<std::size_t>(slot)];
  state.data = std::move(stored.data);
  state.revision = stored.revision;
  state.appliedRevision = 0;
  state.pluginPath = stored.pluginPath;
  state.pluginId = stored.pluginId;
  state.pluginName = stored.pluginName;
  return true;
}

}  // namespace ihub::session
