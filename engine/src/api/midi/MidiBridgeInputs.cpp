#include "api/MidiBridge.h"

#include "api/midi/MidiBridgeSupport.h"
#include "controller/LaunchpadDriver.h"

#include <algorithm>

namespace Api {

bool MidiBridge::openInput(const MidiInputDevice& device, std::string& error) {
    if (inputBackend == nullptr) {
        error = "the MIDI input backend is not available";
        return false;
    }

    auto input = inputBackend->open(device.id);

    if (input == nullptr) {
        error = "cannot open MIDI input '" + device.name + "'";
        return false;
    }

    input->start();

    const std::lock_guard<std::mutex> lock(monitorMutex);
    monitors[device.id].input = std::move(input);
    return true;
}

std::string MidiBridge::roleForName(const juce::String& name) {
    if (Controller::LaunchpadDriver::isLaunchpadName(name)) return "launchpad";

    return "keyboard";
}

std::string MidiBridge::portForName(const juce::String& name) {
    const auto token = Detail::lastToken(name);

    if (token == "MI" || token == "DA") return token;

    if (Detail::containsIgnoringCase(name, "usb")) return "USB";

    return {};
}

std::vector<MidiInputState> MidiBridge::enumerateInputs() {
    std::vector<MidiInputState> inputs;

    for (const auto& device : listJuceMidiInputDevices())
        inputs.push_back(Detail::baseStateFor(device));

    return inputs;
}

bool MidiBridge::setInputEnabled(const std::string& id, bool enabled, std::string& error) {
    if (isNetworkId(id)) {
        if (!setNetworkEnabled(id.substr(4), enabled)) {
            error = "unknown MIDI input: " + id;
            return false;
        }

        return true;
    }

    const auto inputs = listDevices();
    const auto device =
        std::find_if(inputs.begin(), inputs.end(),
                     [&id](const MidiInputDevice& input) { return input.id == id; });

    if (device == inputs.end()) {
        error = "unknown MIDI input: " + id;
        return false;
    }

    if (roleForName(juce::String(device->name)) == "launchpad") {
        auto* driver = launchpad.load(std::memory_order_acquire);

        if (driver == nullptr) {
            error = "the Launchpad driver is not attached";
            return false;
        }

        if (portForName(juce::String(device->name)) == "DA") {
            error =
                "the Launchpad grid uses the MI port; the DA port carries no "
                "pads or "
                "LEDs";
            return false;
        }

        if (enabled == driver->isConnected()) return true;

        if (enabled) {
            if (!driver->connect()) {
                error = Detail::toStd(driver->getLastError());

                if (error.empty()) error = "could not connect the Launchpad";

                return false;
            }
        } else {
            driver->disconnect();
        }

        return true;
    }

    if (enabled) {
        {
            const std::lock_guard<std::mutex> lock(monitorMutex);
            const auto existing = monitors.find(id);

            if (existing != monitors.end() && existing->second.input != nullptr) {
                rememberInput(*device);
                return true;
            }
        }

        if (!openInput(*device, error)) return false;

        rememberInput(*device);
        return true;
    }

    std::unique_ptr<MidiInputHandle> closing;

    {
        const std::lock_guard<std::mutex> lock(monitorMutex);
        const auto existing = monitors.find(id);

        if (existing != monitors.end()) {
            closing = std::move(existing->second.input);
            monitors.erase(existing);
        }
    }

    if (closing != nullptr) closing->stop();

    forgetInput(*device);
    return true;
}

}  // namespace Api
