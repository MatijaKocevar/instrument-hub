#include "MidiBridge.h"

namespace ihub::session {

void MidiBridge::prepare() {
  blockMidi.clear();

  blockMidi.ensureSize(static_cast<std::size_t>(queueCapacity) * 12u + 64u);

  fifo.reset();
}

void MidiBridge::reset() noexcept {
  fifo.reset();
  blockMidi.clear();
}

bool MidiBridge::push(const juce::MidiMessage& message) noexcept {
  const auto size = message.getRawDataSize();

  if (size <= 0 || size > 3 || message.isSysEx()) return false;

  const auto scope = fifo.write(1);

  if (scope.blockSize1 <= 0) return false;

  auto& event = queue[static_cast<std::size_t>(scope.startIndex1)];
  event.size = static_cast<std::uint8_t>(size);

  const auto* raw = message.getRawData();

  for (int index = 0; index < size; ++index)
    event.data[static_cast<std::size_t>(index)] = raw[index];

  return true;
}

void MidiBridge::drain(juce::MidiBuffer& destination) noexcept {
  const auto ready = fifo.getNumReady();

  if (ready <= 0) return;

  const auto scope = fifo.read(ready);

  auto add = [this, &destination](int start, int count) {
    for (int index = 0; index < count; ++index) {
      const auto& event = queue[static_cast<std::size_t>(start + index)];

      if (event.size > 0)
        destination.addEvent(event.data.data(), static_cast<int>(event.size),
                             0);
    }
  };

  add(scope.startIndex1, scope.blockSize1);
  add(scope.startIndex2, scope.blockSize2);
}

}  // namespace ihub::session
