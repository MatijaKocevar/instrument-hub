#include "controller/LaunchpadDriver.h"

#include <span>

namespace Controller {

void LaunchpadDriver::setMessageSink(MessageSink sink) {
  const std::lock_guard<std::mutex> lock(sinkMutex);
  messageSink = std::move(sink);
}

void LaunchpadDriver::injectMidiMessage(const juce::MidiMessage& message) {
  handleIncomingMidiMessage(nullptr, message);
}

void LaunchpadDriver::handleIncomingMidiMessage(
    juce::MidiInput*, const juce::MidiMessage& message) {
  if (message.isSysEx()) {
    const auto bytes = std::span<const std::uint8_t>(
        message.getRawData(),
        static_cast<std::size_t>(message.getRawDataSize()));

    if (Launchpad::isProgrammerModeFrame(bytes)) handshakeEcho.store(true);

    return;
  }

  if (message.isNoteOnOrOff()) {
    const int pad = Launchpad::padIndexForNote(message.getNoteNumber());

    if (pad >= 0)
      dispatchPad(pad, message.isNoteOn() && message.getVelocity() > 0);

    return;
  }

  if (message.isController()) {
    const int cc = message.getControllerNumber();

    if (Launchpad::topRowIndexForCC(cc).has_value() ||
        Launchpad::rightColumnIndexForCC(cc).has_value() ||
        cc == Launchpad::logoCC) {
      dispatchButton(cc, message.getControllerValue() > 0);
    }
  }
}

void LaunchpadDriver::dispatchPad(int padIndex, bool pressed) {
  DriverListeners copy;

  {
    const std::lock_guard<std::mutex> lock(listenerMutex);
    copy = listeners;
  }

  if (copy.padEvent) copy.padEvent(padIndex, pressed);
}

void LaunchpadDriver::dispatchButton(int cc, bool pressed) {
  DriverListeners copy;

  {
    const std::lock_guard<std::mutex> lock(listenerMutex);
    copy = listeners;
  }

  if (copy.buttonEvent) copy.buttonEvent(cc, pressed);
}

}  // namespace Controller
