#include "controller/LaunchpadDriver.h"

namespace Controller {

namespace {
constexpr std::uint16_t packLed(PatchLed led) noexcept {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(led.mode) << 8) |
                                      static_cast<std::uint16_t>(led.velocity));
}

constexpr PatchLed unpackLed(std::uint16_t packed) noexcept {
    return {static_cast<std::uint8_t>(packed & 0xFFu),
            static_cast<Launchpad::LedMode>((packed >> 8) & 0xFFu)};
}
}  // namespace

void LaunchpadDriver::setPadState(int padIndex, PadState state) {
    if (padIndex < 0 || padIndex >= Launchpad::padCount) return;

    padStates[static_cast<std::size_t>(padIndex)].store(static_cast<std::uint8_t>(state));

    setPadLed(padIndex,
              {Launchpad::velocityFor(launchpadColourForState(state)), Launchpad::LedMode::Static});
}

PadState LaunchpadDriver::getPadState(int padIndex) const noexcept {
    if (padIndex < 0 || padIndex >= Launchpad::padCount) return PadState::idle;

    return static_cast<PadState>(padStates[static_cast<std::size_t>(padIndex)].load());
}

void LaunchpadDriver::setPadLed(int padIndex, PatchLed led) {
    if (padIndex < 0 || padIndex >= Launchpad::padCount) return;

    auto& slot = padLeds[static_cast<std::size_t>(padIndex)];
    auto& rgbSlot = padRgb[static_cast<std::size_t>(padIndex)];
    const auto previousRgb = rgbSlot.exchange(0);
    const auto packed = packLed(led);

    if (slot.exchange(packed) == packed && previousRgb == 0) return;

    sendPadVelocity(padIndex, led.velocity, led.mode);
}

void LaunchpadDriver::setPadRgb(int padIndex, std::uint8_t red, std::uint8_t green,
                                std::uint8_t blue, std::uint8_t snapshotVelocity) {
    if (padIndex < 0 || padIndex >= Launchpad::padCount) return;

    const std::uint32_t packed = 0x80000000u | (static_cast<std::uint32_t>(red) << 16) |
                                 (static_cast<std::uint32_t>(green) << 8) |
                                 static_cast<std::uint32_t>(blue);

    padLeds[static_cast<std::size_t>(padIndex)].store(
        packLed({snapshotVelocity, Launchpad::LedMode::Static}));

    auto& slot = padRgb[static_cast<std::size_t>(padIndex)];

    if (slot.exchange(packed) == packed) return;

    sendSysEx(Launchpad::rgbPadFrame(padIndex, red, green, blue));
}

PatchLed LaunchpadDriver::getPadLed(int padIndex) const noexcept {
    if (padIndex < 0 || padIndex >= Launchpad::padCount) return {};

    return unpackLed(padLeds[static_cast<std::size_t>(padIndex)].load());
}

void LaunchpadDriver::setPatchLed(int padIndex, int colorIndex, bool armed, bool warming,
                                  bool error) {
    setPadLed(padIndex, launchpadLedForPatch(colorIndex, armed, warming, error));
}

void LaunchpadDriver::setAllPadStates(PadState state) {
    for (int pad = 0; pad < Launchpad::padCount; ++pad) setPadState(pad, state);
}

int LaunchpadDriver::controlButtonSlot(int cc) noexcept {
    if (const auto topRow = Launchpad::topRowIndexForCC(cc)) return *topRow;

    if (const auto rightColumn = Launchpad::rightColumnIndexForCC(cc))
        return Launchpad::topRowCount + *rightColumn;

    return -1;
}

void LaunchpadDriver::setButtonState(int cc, PadState state) {
    const int slot = controlButtonSlot(cc);

    if (slot < 0) return;

    buttonStates[static_cast<std::size_t>(slot)].store(static_cast<std::uint8_t>(state));

    sendButtonColour(cc, launchpadColourForState(state));
}

PadState LaunchpadDriver::getButtonState(int cc) const noexcept {
    const int slot = controlButtonSlot(cc);

    if (slot < 0) return PadState::idle;

    return static_cast<PadState>(buttonStates[static_cast<std::size_t>(slot)].load());
}

void LaunchpadDriver::setAllButtonStates(PadState state) {
    for (int index = 0; index < Launchpad::topRowCount; ++index)
        buttonStates[static_cast<std::size_t>(index)].store(static_cast<std::uint8_t>(state));

    for (int index = 0; index < Launchpad::rightColumnCount; ++index)
        buttonStates[static_cast<std::size_t>(Launchpad::topRowCount + index)].store(
            static_cast<std::uint8_t>(state));

    for (int index = 0; index < Launchpad::topRowCount; ++index)
        sendButtonColour(Launchpad::topRowCCForIndex(index), launchpadColourForState(state));

    for (int index = 0; index < Launchpad::rightColumnCount; ++index)
        sendButtonColour(Launchpad::rightColumnCCForIndex(index), launchpadColourForState(state));
}

void LaunchpadDriver::refreshLeds() {
    for (int pad = 0; pad < Launchpad::padCount; ++pad) {
        const auto packed = padRgb[static_cast<std::size_t>(pad)].load();

        if (packed != 0) {
            const auto red = static_cast<std::uint8_t>((packed >> 16) & 0xFFu);
            const auto green = static_cast<std::uint8_t>((packed >> 8) & 0xFFu);
            const auto blue = static_cast<std::uint8_t>(packed & 0xFFu);
            sendSysEx(Launchpad::rgbPadFrame(pad, red, green, blue));
            continue;
        }

        const auto led = getPadLed(pad);
        sendPadVelocity(pad, led.velocity, led.mode);
    }

    for (int index = 0; index < Launchpad::topRowCount; ++index) {
        const int cc = Launchpad::topRowCCForIndex(index);
        sendButtonColour(cc, launchpadColourForState(getButtonState(cc)));
    }

    for (int index = 0; index < Launchpad::rightColumnCount; ++index) {
        const int cc = Launchpad::rightColumnCCForIndex(index);
        sendButtonColour(cc, launchpadColourForState(getButtonState(cc)));
    }
}

void LaunchpadDriver::clearLeds() {
    for (int pad = 0; pad < Launchpad::padCount; ++pad) {
        padStates[static_cast<std::size_t>(pad)].store(static_cast<std::uint8_t>(PadState::idle));
        padLeds[static_cast<std::size_t>(pad)].store(0);
        padRgb[static_cast<std::size_t>(pad)].store(0);
        sendPadColour(pad, Launchpad::Colour::Off);
    }

    for (auto& state : buttonStates) state.store(static_cast<std::uint8_t>(PadState::idle));

    for (int index = 0; index < Launchpad::topRowCount; ++index)
        sendButtonColour(Launchpad::topRowCCForIndex(index), Launchpad::Colour::Off);

    for (int index = 0; index < Launchpad::rightColumnCount; ++index)
        sendButtonColour(Launchpad::rightColumnCCForIndex(index), Launchpad::Colour::Off);
}

bool LaunchpadDriver::sendPadColour(int padIndex, Launchpad::Colour colour,
                                    Launchpad::LedMode mode) {
    return sendPadVelocity(padIndex, Launchpad::velocityFor(colour), mode);
}

bool LaunchpadDriver::sendPadVelocity(int padIndex, std::uint8_t velocity,
                                      Launchpad::LedMode mode) {
    if (padIndex < 0 || padIndex >= Launchpad::padCount) return false;

    const auto bytes = Launchpad::padVelocityMessage(padIndex, velocity, mode);
    return sendBytes(bytes.data(), static_cast<int>(bytes.size()));
}

bool LaunchpadDriver::sendButtonColour(int cc, Launchpad::Colour colour, Launchpad::LedMode mode) {
    const auto bytes = Launchpad::buttonColourMessage(cc, colour, mode);

    if (bytes == std::array<std::uint8_t, 3>{0, 0, 0}) return false;

    return sendBytes(bytes.data(), static_cast<int>(bytes.size()));
}

bool LaunchpadDriver::sendSysEx(const Launchpad::SysExFrame& frame) {
    if (!frame.isValid()) return false;

    return sendBytes(frame.bytes.data(), static_cast<int>(frame.bytes.size()));
}

}  // namespace Controller
