#include "api/MidiBridge.h"

#include "api/midi/MidiBridgeSupport.h"
#include "controller/LaunchpadDriver.h"

namespace Api {

MidiInputState MidiBridge::makeState(const MidiInputDevice& device) const {
  auto state = Detail::baseStateFor(device);

  if (state.role == "launchpad") {
    auto* driver = launchpad.load(std::memory_order_acquire);
    const bool driverOpen = driver != nullptr && driver->isConnected();
    const bool miPort = state.port == "MI";
    const bool ownedPort =
        driverOpen &&
        (miPort || state.name == Detail::toStd(driver->getInputName()));

    state.enabled = ownedPort;
    state.open = ownedPort;
    state.leds = miPort;
    state.handshake = ownedPort && driver->hasReceivedHandshakeEcho();

    if (state.port == "DA") {
      state.error =
          "the DA port carries no pads or LEDs; the grid uses the MI port";
    } else if (driver == nullptr) {
      state.error = "the Launchpad driver is not attached";
    } else if (!driverOpen) {
      const auto driverError = Detail::toStd(driver->getLastError());
      state.error = driverError.empty()
                        ? "the Launchpad is not connected; check its MI port"
                        : driverError;
    }

    return state;
  }

  {
    const std::lock_guard<std::mutex> lock(monitorMutex);
    const auto monitor = monitors.find(device.id);

    if (monitor != monitors.end()) {
      state.enabled = true;
      state.open = monitor->second.input != nullptr;
      state.events = monitor->second.events;
      state.lastEvent = monitor->second.lastEvent;
    }
  }

  MidiGridMap map;
  bool isGrid = false;
  std::string gridError;

  {
    const std::lock_guard<std::mutex> lock(gridMutex);
    const auto grid = grids.find(device.id);

    if (grid != grids.end()) {
      isGrid = true;
      map = grid->second.map;
      gridError = grid->second.error;
    }
  }

  if (!isGrid) return state;

  state.role = "grid";
  state.noteBase = map.noteBase();
  state.learnedNotes = map.learnedCount();
  state.learnActive = map.isLearning();

  if (!gridError.empty()) {
    state.error = gridError;
  } else if (!map.isLearning() && !map.isConfigured()) {
    state.error = "set a note base or learn all 64 pads before using this grid";
  }

  return state;
}

MidiStateSnapshot MidiBridge::snapshot() {
  restoreStoredInputs();
  restoreGridConfigs();
  expireLearnTimeouts();
  persistGridSettings();

  MidiStateSnapshot state;

  for (const auto& device : listDevices())
    state.inputs.push_back(makeState(device));

  return state;
}

}  // namespace Api
