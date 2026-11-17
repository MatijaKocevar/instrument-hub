#include "LaunchpadProtocol.h"

#include <algorithm>

namespace Launchpad {

bool SysExFrame::isValid() const noexcept {
  if (bytes.size() < sysExHeader.size() + 2) return false;

  if (bytes.front() != sysExStart || bytes.back() != sysExEnd) return false;

  if (!std::equal(sysExHeader.begin(), sysExHeader.end(), bytes.begin()))
    return false;

  return std::all_of(
      bytes.begin() + static_cast<std::ptrdiff_t>(sysExHeader.size()),
      bytes.end() - 1, [](std::uint8_t byte) { return byte <= 0x7F; });
}

SysExFrame makeSysExFrame(SysExCommand command,
                          std::span<const std::uint8_t> payload) {
  const bool payloadIsSevenBit =
      std::all_of(payload.begin(), payload.end(),
                  [](std::uint8_t byte) { return byte <= 0x7F; });

  if (!payloadIsSevenBit) return {};

  SysExFrame frame;
  frame.bytes.reserve(sysExHeader.size() + payload.size() + 2);
  frame.bytes.insert(frame.bytes.end(), sysExHeader.begin(), sysExHeader.end());
  frame.bytes.push_back(static_cast<std::uint8_t>(command));
  frame.bytes.insert(frame.bytes.end(), payload.begin(), payload.end());
  frame.bytes.push_back(sysExEnd);

  return frame;
}

SysExFrame programmerModeFrame(bool enabled) {
  const std::array<std::uint8_t, 1> payload{
      static_cast<std::uint8_t>(enabled ? 1 : 0)};

  return makeSysExFrame(SysExCommand::ProgrammerMode, payload);
}

SysExFrame selectLayoutFrame(Layout layout) {
  const std::array<std::uint8_t, 1> payload{static_cast<std::uint8_t>(layout)};

  return makeSysExFrame(SysExCommand::SelectLayout, payload);
}

SysExFrame ledFeedbackFrame(bool internal, bool external) {
  const std::array<std::uint8_t, 2> payload{
      static_cast<std::uint8_t>(internal ? 1 : 0),
      static_cast<std::uint8_t>(external ? 1 : 0)};

  return makeSysExFrame(SysExCommand::LedFeedback, payload);
}

SysExFrame ledSleepFrame(bool awake) {
  const std::array<std::uint8_t, 1> payload{
      static_cast<std::uint8_t>(awake ? 1 : 0)};

  return makeSysExFrame(SysExCommand::LedSleep, payload);
}

namespace {
std::uint8_t toSevenBit(std::uint8_t value) noexcept {
  return static_cast<std::uint8_t>((static_cast<int>(value) * 127 + 127) / 255);
}
}  // namespace

SysExFrame rgbPadFrame(int padIndex, std::uint8_t red, std::uint8_t green,
                       std::uint8_t blue) {
  const int led = padNoteForIndex(padIndex);

  if (led < 0 || led > 127) return {};

  const std::array<std::uint8_t, 5> payload{
      0x03, static_cast<std::uint8_t>(led), toSevenBit(red), toSevenBit(green),
      toSevenBit(blue)};

  return makeSysExFrame(SysExCommand::LedLighting, payload);
}

bool isProgrammerModeFrame(std::span<const std::uint8_t> bytes) {
  if (bytes.size() != sysExHeader.size() + 3) return false;

  if (bytes.front() != sysExStart || bytes.back() != sysExEnd) return false;

  if (!std::equal(sysExHeader.begin(), sysExHeader.end(), bytes.begin()))
    return false;

  return bytes[sysExHeader.size()] ==
             static_cast<std::uint8_t>(SysExCommand::ProgrammerMode) &&
         bytes[sysExHeader.size() + 1] <= 1;
}

}  // namespace Launchpad
