#include "controller/LaunchpadDriver.h"

namespace Controller {

bool LaunchpadDriver::sendBytes(const std::uint8_t* bytes, int size) {
  if (bytes == nullptr || size <= 0) return false;

  const juce::MidiMessage message(bytes, size);
  MessageSink sink;
  bool sent = false;

  {
    const std::lock_guard<std::mutex> lock(sendMutex);

    if (output != nullptr) {
      output->sendMessageNow(message);
      sent = true;
    }

    const std::lock_guard<std::mutex> sinkLock(sinkMutex);
    sink = messageSink;
  }

  if (sink) sink(message);

  return sent || sink != nullptr;
}

void LaunchpadDriver::setError(const juce::String& message) {
  const std::lock_guard<std::mutex> lock(errorMutex);
  lastError = message;
}

void LaunchpadDriver::clearError() {
  const std::lock_guard<std::mutex> lock(errorMutex);
  lastError.clear();
}

}  // namespace Controller
