#include "controller/LaunchpadDriver.h"

#include <utility>

namespace Controller {

LaunchpadDriver::LaunchpadDriver() {
    for (auto& state : padStates) state.store(static_cast<std::uint8_t>(PadState::idle));

    for (auto& led : padLeds) led.store(0);

    for (auto& rgb : padRgb) rgb.store(0);

    for (auto& state : buttonStates) state.store(static_cast<std::uint8_t>(PadState::idle));
}

LaunchpadDriver::~LaunchpadDriver() { disconnect(); }

void LaunchpadDriver::setListeners(DriverListeners newListeners) {
    const std::lock_guard<std::mutex> lock(listenerMutex);
    listeners = std::move(newListeners);
}

bool LaunchpadDriver::connect() {
    const auto selection = findDevice();

    if (!selection.has_value()) {
        setError("no Launchpad Mini MK3 input/output pair found");
        return false;
    }

    return connect(*selection);
}

bool LaunchpadDriver::connect(const DeviceSelection& selection) {
    disconnect();

    if (selection.inputIdentifier.isEmpty() || selection.outputIdentifier.isEmpty()) {
        setError("the Launchpad device selection is incomplete");
        return false;
    }

    auto newInput = juce::MidiInput::openDevice(selection.inputIdentifier, this);

    if (newInput == nullptr) {
        setError("cannot open MIDI input '" + selection.inputName + "'");
        return false;
    }

    auto newOutput = juce::MidiOutput::openDevice(selection.outputIdentifier);

    if (newOutput == nullptr) {
        setError("cannot open MIDI output '" + selection.outputName + "'");
        return false;
    }

    input = std::move(newInput);
    output = std::move(newOutput);

    inputName = input->getDeviceInfo().name;
    outputName = output->getDeviceInfo().name;

    handshakeEcho.store(false);
    connected.store(true);

    input->start();

    if (!sendHandshake()) {
        setError("the programmer-mode handshake could not be sent");
        disconnect();
        return false;
    }

    clearLeds();

    clearError();
    return true;
}

void LaunchpadDriver::disconnect() {
    const bool wasConnected = connected.exchange(false);

    if (wasConnected) sendSysEx(Launchpad::programmerModeFrame(false));

    if (input != nullptr) {
        input->stop();
        input.reset();
    }

    output.reset();
}

bool LaunchpadDriver::isConnected() const noexcept { return connected.load(); }

bool LaunchpadDriver::canSendLeds() const noexcept {
    if (connected.load()) return true;

    const std::lock_guard<std::mutex> lock(sinkMutex);
    return messageSink != nullptr;
}

juce::String LaunchpadDriver::getInputName() const { return inputName; }

juce::String LaunchpadDriver::getOutputName() const { return outputName; }

juce::String LaunchpadDriver::getLastError() const {
    const std::lock_guard<std::mutex> lock(errorMutex);
    return lastError;
}

bool LaunchpadDriver::sendHandshake() {
    const bool modeOk = sendSysEx(Launchpad::programmerModeFrame(true));
    const bool feedbackOk = sendSysEx(Launchpad::ledFeedbackFrame(false, true));
    const bool wakeOk = sendSysEx(Launchpad::ledSleepFrame(true));

    return modeOk && feedbackOk && wakeOk;
}

bool LaunchpadDriver::hasReceivedHandshakeEcho() const noexcept { return handshakeEcho.load(); }

}  // namespace Controller
