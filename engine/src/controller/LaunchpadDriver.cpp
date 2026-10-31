#include "LaunchpadDriver.h"

#include <algorithm>

namespace Controller
{

namespace
{
/** Name fragment shared by the hardware and any simulator. */
constexpr const char* launchpadNameFragment = "launchpad mini mk3";

bool nameMatches (const juce::String& name)
{
    return name.toLowerCase().contains (launchpadNameFragment);
}

/** The MI port carries pad input and LED output; the DA port does not.
    Match the port suffix, not the "mi" inside "Mini". */
bool isMiPort (const juce::String& name)
{
    const auto lower = name.toLowerCase();
    return lower.contains ("mk3 mi") || lower.endsWith ("mi")
           || lower.contains ("lpminimk3 mi");
}

bool isDawPort (const juce::String& name)
{
    const auto lower = name.toLowerCase();
    return lower.contains ("mk3 da") || lower.endsWith ("da")
           || lower.contains ("daw");
}

const juce::MidiDeviceInfo* chooseDevice (const juce::Array<juce::MidiDeviceInfo>& devices)
{
    const juce::MidiDeviceInfo* firstMatch = nullptr;

    for (const auto& device : devices)
    {
        if (! nameMatches (device.name))
            continue;

        if (isMiPort (device.name))
            return &device;

        if (firstMatch == nullptr)
            firstMatch = &device;
    }

    // No MI port visible: accept a matching port that is not the inert DA one.
    for (const auto& device : devices)
        if (nameMatches (device.name) && ! isDawPort (device.name))
            return &device;

    return firstMatch;
}
} // namespace

//==============================================================================
Launchpad::Colour launchpadColourForState (PadState state) noexcept
{
    switch (state)
    {
        case PadState::idle:    return Launchpad::Colour::Off;
        case PadState::warming: return Launchpad::Colour::Yellow;
        case PadState::active:  return Launchpad::Colour::Green;
        case PadState::error:   return Launchpad::Colour::Red;
    }

    return Launchpad::Colour::Off;
}

//==============================================================================
// Detection
//==============================================================================

bool LaunchpadDriver::isLaunchpadName (const juce::String& deviceName)
{
    return nameMatches (deviceName);
}

std::optional<LaunchpadDriver::DeviceSelection> LaunchpadDriver::findDevice()
{
    const auto inputs = juce::MidiInput::getAvailableDevices();
    const auto outputs = juce::MidiOutput::getAvailableDevices();

    const auto* input = chooseDevice (inputs);
    const auto* output = chooseDevice (outputs);

    if (input == nullptr || output == nullptr)
        return std::nullopt;

    DeviceSelection selection;
    selection.inputIdentifier = input->identifier;
    selection.outputIdentifier = output->identifier;
    selection.inputName = input->name;
    selection.outputName = output->name;
    return selection;
}

//==============================================================================
// Lifecycle
//==============================================================================

LaunchpadDriver::LaunchpadDriver()
{
    for (auto& state : padStates)
        state.store (static_cast<std::uint8_t> (PadState::idle));

    for (auto& state : buttonStates)
        state.store (static_cast<std::uint8_t> (PadState::idle));
}

LaunchpadDriver::~LaunchpadDriver()
{
    disconnect();
}

void LaunchpadDriver::setListeners (DriverListeners newListeners)
{
    const std::lock_guard<std::mutex> lock (listenerMutex);
    listeners = std::move (newListeners);
}

bool LaunchpadDriver::connect()
{
    const auto selection = findDevice();

    if (! selection.has_value())
    {
        setError ("no Launchpad Mini MK3 input/output pair found");
        return false;
    }

    return connect (*selection);
}

bool LaunchpadDriver::connect (const DeviceSelection& selection)
{
    disconnect();

    if (selection.inputIdentifier.isEmpty() || selection.outputIdentifier.isEmpty())
    {
        setError ("the Launchpad device selection is incomplete");
        return false;
    }

    auto newInput = juce::MidiInput::openDevice (selection.inputIdentifier, this);

    if (newInput == nullptr)
    {
        setError ("cannot open MIDI input '" + selection.inputName + "'");
        return false;
    }

    auto newOutput = juce::MidiOutput::openDevice (selection.outputIdentifier);

    if (newOutput == nullptr)
    {
        setError ("cannot open MIDI output '" + selection.outputName + "'");
        return false;
    }

    input = std::move (newInput);
    output = std::move (newOutput);

    inputName = input->getDeviceInfo().name;
    outputName = output->getDeviceInfo().name;

    handshakeEcho.store (false);
    connected.store (true);

    input->start();

    if (! sendHandshake())
    {
        setError ("the programmer-mode handshake could not be sent");
        disconnect();
        return false;
    }

    clearError();
    return true;
}

void LaunchpadDriver::disconnect()
{
    const bool wasConnected = connected.exchange (false);

    // Leave programmer mode before closing, so the surface returns to Live
    // mode instead of staying unlit with the setup menu disabled.
    if (wasConnected)
        sendSysEx (Launchpad::programmerModeFrame (false));

    if (input != nullptr)
    {
        input->stop();
        input.reset();
    }

    output.reset();
}

bool LaunchpadDriver::isConnected() const noexcept
{
    return connected.load();
}

juce::String LaunchpadDriver::getInputName() const
{
    return inputName;
}

juce::String LaunchpadDriver::getOutputName() const
{
    return outputName;
}

juce::String LaunchpadDriver::getLastError() const
{
    const std::lock_guard<std::mutex> lock (errorMutex);
    return lastError;
}

//==============================================================================
// Handshake
//==============================================================================

bool LaunchpadDriver::sendHandshake()
{
    const bool modeOk = sendSysEx (Launchpad::programmerModeFrame (true));
    const bool feedbackOk = sendSysEx (Launchpad::ledFeedbackFrame (false, true));
    const bool wakeOk = sendSysEx (Launchpad::ledSleepFrame (true));

    return modeOk && feedbackOk && wakeOk;
}

bool LaunchpadDriver::hasReceivedHandshakeEcho() const noexcept
{
    return handshakeEcho.load();
}

//==============================================================================
// LEDs
//==============================================================================

void LaunchpadDriver::setPadState (int padIndex, PadState state)
{
    if (padIndex < 0 || padIndex >= Launchpad::padCount)
        return;

    padStates[static_cast<std::size_t> (padIndex)].store (
        static_cast<std::uint8_t> (state));

    sendPadColour (padIndex, launchpadColourForState (state));
}

PadState LaunchpadDriver::getPadState (int padIndex) const noexcept
{
    if (padIndex < 0 || padIndex >= Launchpad::padCount)
        return PadState::idle;

    return static_cast<PadState> (
        padStates[static_cast<std::size_t> (padIndex)].load());
}

void LaunchpadDriver::setAllPadStates (PadState state)
{
    for (int pad = 0; pad < Launchpad::padCount; ++pad)
        padStates[static_cast<std::size_t> (pad)].store (
            static_cast<std::uint8_t> (state));

    refreshLeds();
}

int LaunchpadDriver::controlButtonSlot (int cc) noexcept
{
    if (const auto topRow = Launchpad::topRowIndexForCC (cc))
        return *topRow;

    if (const auto rightColumn = Launchpad::rightColumnIndexForCC (cc))
        return Launchpad::topRowCount + *rightColumn;

    return -1;
}

void LaunchpadDriver::setButtonState (int cc, PadState state)
{
    const int slot = controlButtonSlot (cc);

    if (slot < 0)
        return;

    buttonStates[static_cast<std::size_t> (slot)].store (
        static_cast<std::uint8_t> (state));

    sendButtonColour (cc, launchpadColourForState (state));
}

PadState LaunchpadDriver::getButtonState (int cc) const noexcept
{
    const int slot = controlButtonSlot (cc);

    if (slot < 0)
        return PadState::idle;

    return static_cast<PadState> (
        buttonStates[static_cast<std::size_t> (slot)].load());
}

void LaunchpadDriver::setAllButtonStates (PadState state)
{
    for (int index = 0; index < Launchpad::topRowCount; ++index)
        buttonStates[static_cast<std::size_t> (index)].store (
            static_cast<std::uint8_t> (state));

    for (int index = 0; index < Launchpad::rightColumnCount; ++index)
        buttonStates[static_cast<std::size_t> (Launchpad::topRowCount + index)]
            .store (static_cast<std::uint8_t> (state));

    for (int index = 0; index < Launchpad::topRowCount; ++index)
        sendButtonColour (Launchpad::topRowCCForIndex (index),
                          launchpadColourForState (state));

    for (int index = 0; index < Launchpad::rightColumnCount; ++index)
        sendButtonColour (Launchpad::rightColumnCCForIndex (index),
                          launchpadColourForState (state));
}

void LaunchpadDriver::refreshLeds()
{
    for (int pad = 0; pad < Launchpad::padCount; ++pad)
        sendPadColour (pad, launchpadColourForState (getPadState (pad)));

    for (int index = 0; index < Launchpad::topRowCount; ++index)
    {
        const int cc = Launchpad::topRowCCForIndex (index);
        sendButtonColour (cc, launchpadColourForState (getButtonState (cc)));
    }

    for (int index = 0; index < Launchpad::rightColumnCount; ++index)
    {
        const int cc = Launchpad::rightColumnCCForIndex (index);
        sendButtonColour (cc, launchpadColourForState (getButtonState (cc)));
    }
}

void LaunchpadDriver::clearLeds()
{
    for (int pad = 0; pad < Launchpad::padCount; ++pad)
    {
        padStates[static_cast<std::size_t> (pad)].store (
            static_cast<std::uint8_t> (PadState::idle));
        sendPadColour (pad, Launchpad::Colour::Off);
    }

    for (auto& state : buttonStates)
        state.store (static_cast<std::uint8_t> (PadState::idle));

    for (int index = 0; index < Launchpad::topRowCount; ++index)
        sendButtonColour (Launchpad::topRowCCForIndex (index),
                          Launchpad::Colour::Off);

    for (int index = 0; index < Launchpad::rightColumnCount; ++index)
        sendButtonColour (Launchpad::rightColumnCCForIndex (index),
                          Launchpad::Colour::Off);
}

bool LaunchpadDriver::sendPadColour (int padIndex,
                                     Launchpad::Colour colour,
                                     Launchpad::LedMode mode)
{
    if (padIndex < 0 || padIndex >= Launchpad::padCount)
        return false;

    const auto bytes = Launchpad::padColourMessage (padIndex, colour, mode);
    return sendBytes (bytes.data(), static_cast<int> (bytes.size()));
}

bool LaunchpadDriver::sendButtonColour (int cc,
                                        Launchpad::Colour colour,
                                        Launchpad::LedMode mode)
{
    const auto bytes = Launchpad::buttonColourMessage (cc, colour, mode);

    if (bytes == std::array<std::uint8_t, 3> { 0, 0, 0 })
        return false;

    return sendBytes (bytes.data(), static_cast<int> (bytes.size()));
}

bool LaunchpadDriver::sendSysEx (const Launchpad::SysExFrame& frame)
{
    if (! frame.isValid())
        return false;

    return sendBytes (frame.bytes.data(), static_cast<int> (frame.bytes.size()));
}

//==============================================================================
// Scripted operation
//==============================================================================

void LaunchpadDriver::setMessageSink (MessageSink sink)
{
    const std::lock_guard<std::mutex> lock (sinkMutex);
    messageSink = std::move (sink);
}

void LaunchpadDriver::injectMidiMessage (const juce::MidiMessage& message)
{
    handleIncomingMidiMessage (nullptr, message);
}

//==============================================================================
// Incoming MIDI
//==============================================================================

void LaunchpadDriver::handleIncomingMidiMessage (juce::MidiInput*,
                                                 const juce::MidiMessage& message)
{
    if (message.isSysEx())
    {
        const auto bytes = std::span<const std::uint8_t> (
            message.getRawData(),
            static_cast<std::size_t> (message.getRawDataSize()));

        if (Launchpad::isProgrammerModeFrame (bytes))
            handshakeEcho.store (true);

        return;
    }

    if (message.isNoteOnOrOff())
    {
        const int pad = Launchpad::padIndexForNote (message.getNoteNumber());

        if (pad >= 0)
            dispatchPad (pad, message.isNoteOn() && message.getVelocity() > 0);

        return;
    }

    if (message.isController())
    {
        const int cc = message.getControllerNumber();

        if (Launchpad::topRowIndexForCC (cc).has_value()
            || Launchpad::rightColumnIndexForCC (cc).has_value()
            || cc == Launchpad::logoCC)
        {
            dispatchButton (cc, message.getControllerValue() > 0);
        }
    }
}

void LaunchpadDriver::dispatchPad (int padIndex, bool pressed)
{
    DriverListeners copy;

    {
        const std::lock_guard<std::mutex> lock (listenerMutex);
        copy = listeners;
    }

    if (copy.padEvent)
        copy.padEvent (padIndex, pressed);
}

void LaunchpadDriver::dispatchButton (int cc, bool pressed)
{
    DriverListeners copy;

    {
        const std::lock_guard<std::mutex> lock (listenerMutex);
        copy = listeners;
    }

    if (copy.buttonEvent)
        copy.buttonEvent (cc, pressed);
}

//==============================================================================
// Low-level output
//==============================================================================

bool LaunchpadDriver::sendBytes (const std::uint8_t* bytes, int size)
{
    if (bytes == nullptr || size <= 0)
        return false;

    const juce::MidiMessage message (bytes, size);
    MessageSink sink;
    bool sent = false;

    {
        const std::lock_guard<std::mutex> lock (sendMutex);

        if (output != nullptr)
        {
            output->sendMessageNow (message);
            sent = true;
        }

        const std::lock_guard<std::mutex> sinkLock (sinkMutex);
        sink = messageSink;
    }

    if (sink)
        sink (message);

    return sent || sink != nullptr;
}

void LaunchpadDriver::setError (const juce::String& message)
{
    const std::lock_guard<std::mutex> lock (errorMutex);
    lastError = message;
}

void LaunchpadDriver::clearError()
{
    const std::lock_guard<std::mutex> lock (errorMutex);
    lastError.clear();
}

} // namespace Controller
