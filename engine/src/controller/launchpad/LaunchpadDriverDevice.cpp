#include "controller/LaunchpadDriver.h"

namespace Controller {

namespace {
constexpr const char* launchpadNameFragment = "launchpad mini mk3";

bool nameMatches(const juce::String& name) {
    return name.toLowerCase().contains(launchpadNameFragment);
}

bool isMiPort(const juce::String& name) {
    const auto lower = name.toLowerCase();
    return lower.contains("mk3 mi") || lower.endsWith("mi") || lower.contains("lpminimk3 mi");
}

bool isDawPort(const juce::String& name) {
    const auto lower = name.toLowerCase();
    return lower.contains("mk3 da") || lower.endsWith("da") || lower.contains("daw");
}

const juce::MidiDeviceInfo* chooseDevice(const juce::Array<juce::MidiDeviceInfo>& devices) {
    const juce::MidiDeviceInfo* firstMatch = nullptr;

    for (const auto& device : devices) {
        if (!nameMatches(device.name)) continue;

        if (isMiPort(device.name)) return &device;

        if (firstMatch == nullptr) firstMatch = &device;
    }

    for (const auto& device : devices)
        if (nameMatches(device.name) && !isDawPort(device.name)) return &device;

    return firstMatch;
}
}  // namespace

bool LaunchpadDriver::isLaunchpadName(const juce::String& deviceName) {
    return nameMatches(deviceName);
}

std::optional<LaunchpadDriver::DeviceSelection> LaunchpadDriver::findDevice() {
    const auto inputs = juce::MidiInput::getAvailableDevices();
    const auto outputs = juce::MidiOutput::getAvailableDevices();

    const auto* input = chooseDevice(inputs);
    const auto* output = chooseDevice(outputs);

    if (input == nullptr || output == nullptr) return std::nullopt;

    DeviceSelection selection;
    selection.inputIdentifier = input->identifier;
    selection.outputIdentifier = output->identifier;
    selection.inputName = input->name;
    selection.outputName = output->name;
    return selection;
}

}  // namespace Controller
