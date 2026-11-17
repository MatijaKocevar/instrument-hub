#include "MidiDeviceWatcher.h"

#include "api/MidiBridge.h"
#include "api/StateStore.h"

#include <utility>

namespace ihub::app {

MidiDeviceWatcher::MidiDeviceWatcher(Api::StateStore& stateToUse)
    : state(stateToUse) {
  for (const auto& input : Api::MidiBridge::enumerateInputs())
    knownIds.push_back(input.id);

  startTimer(pollIntervalMs);
}

MidiDeviceWatcher::~MidiDeviceWatcher() { stopTimer(); }

void MidiDeviceWatcher::timerCallback() {
  std::vector<std::string> ids;

  for (const auto& input : Api::MidiBridge::enumerateInputs())
    ids.push_back(input.id);

  if (ids == knownIds) return;

  knownIds = std::move(ids);
  state.notifyChanged("midi", -1);
}

}  // namespace ihub::app
