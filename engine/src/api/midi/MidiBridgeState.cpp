#include "api/MidiBridge.h"

#include "api/midi/MidiBridgeSupport.h"
#include "controller/LaunchpadDriver.h"

#include <string>

namespace Api {
namespace {
constexpr const char* networkPrefix = "net:";
}

MidiInputState MidiBridge::makeState(const MidiInputDevice& device) const {
    if (isNetworkId(device.id)) return makeNetworkState(device);

    auto state = Detail::baseStateFor(device);

    if (state.role == "launchpad") {
        auto* driver = launchpad.load(std::memory_order_acquire);
        const bool driverOpen = driver != nullptr && driver->isConnected();
        const bool miPort = state.port == "MI";
        const bool ownedPort =
            driverOpen && (miPort || state.name == Detail::toStd(driver->getInputName()));

        state.enabled = ownedPort;
        state.open = ownedPort;
        state.leds = miPort;
        state.handshake = ownedPort && driver->hasReceivedHandshakeEcho();

        if (state.port == "DA") {
            state.error =
                "the DA port carries no pads or LEDs; the grid uses the MI "
                "port";
        } else if (driver == nullptr) {
            state.error = "the Launchpad driver is not attached";
        } else if (!driverOpen) {
            const auto driverError = Detail::toStd(driver->getLastError());
            state.error = driverError.empty() ? "the Launchpad is not connected; check its MI port"
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

    applyGridState(state, device.id);

    return state;
}

MidiInputState MidiBridge::makeNetworkState(const MidiInputDevice& device) const {
    MidiInputState state;
    state.id = device.id;
    state.name = device.name;
    state.role = "keyboard";
    state.port = "WiFi";

    {
        const auto clientId = device.id.substr(std::string(networkPrefix).size());
        const std::lock_guard<std::mutex> lock(networkMutex);
        const auto client = networkClients.find(clientId);

        if (client == networkClients.end()) return state;

        state.enabled = client->second.enabled;
        state.open = client->second.connected && client->second.enabled;
        state.events = client->second.events;
        state.lastEvent = client->second.lastEvent;
    }

    applyGridState(state, device.id);

    return state;
}

void MidiBridge::applyGridState(MidiInputState& state, const std::string& id) const {
    MidiGridMap map;
    bool isGrid = false;
    std::string gridError;

    {
        const std::lock_guard<std::mutex> lock(gridMutex);
        const auto grid = grids.find(id);

        if (grid != grids.end()) {
            isGrid = true;
            map = grid->second.map;
            gridError = grid->second.error;
        }
    }

    if (!isGrid) return;

    state.role = "grid";
    state.noteBase = map.noteBase();
    state.learnedNotes = map.learnedCount();
    state.learnActive = map.isLearning();

    if (!gridError.empty()) {
        state.error = gridError;
    } else if (!map.isLearning() && !map.isConfigured()) {
        state.error = "set a note base or learn all 64 pads before using this grid";
    }
}

void MidiBridge::appendNetworkInputs(MidiStateSnapshot& state) const {
    std::vector<MidiInputDevice> devices;

    {
        const std::lock_guard<std::mutex> lock(networkMutex);
        devices.reserve(networkClients.size());

        for (const auto& entry : networkClients) {
            if (!entry.second.connected) continue;

            devices.push_back(
                MidiInputDevice{networkPrefix + entry.first, entry.second.name, "WiFi"});
        }
    }

    for (const auto& device : devices) state.inputs.push_back(makeNetworkState(device));
}

MidiStateSnapshot MidiBridge::snapshot() {
    restoreStoredInputs();
    restoreGridConfigs();
    expireLearnTimeouts();
    persistGridSettings();

    MidiStateSnapshot state;

    for (const auto& device : listDevices()) state.inputs.push_back(makeState(device));

    appendNetworkInputs(state);

    return state;
}

}  // namespace Api
