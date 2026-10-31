#include "LaunchkeyDriver.h"

#include <algorithm>

namespace Controller
{

namespace
{
/** Name fragment shared by the hardware and the ALSA simulator. */
constexpr const char* launchkeyNameFragment = "launchkey mini mk3";

bool nameMatches (const juce::String& name)
{
    return name.toLowerCase().contains (launchkeyNameFragment);
}

/** Prefer the DAW port when the device exposes more than one match. */
const juce::MidiDeviceInfo* chooseDevice (const juce::Array<juce::MidiDeviceInfo>& devices)
{
    const juce::MidiDeviceInfo* firstMatch = nullptr;

    for (const auto& device : devices)
    {
        if (! nameMatches (device.name))
            continue;

        if (device.name.toLowerCase().contains ("daw"))
            return &device;

        if (firstMatch == nullptr)
            firstMatch = &device;
    }

    return firstMatch;
}
} // namespace

//==============================================================================
Launchkey::Colour colourForState (PadState state) noexcept
{
    switch (state)
    {
        case PadState::idle:    return Launchkey::Colour::DarkGrey;
        case PadState::warming: return Launchkey::Colour::Orange;
        case PadState::active:  return Launchkey::Colour::Green;
        case PadState::error:   return Launchkey::Colour::Red;
    }

    return Launchkey::Colour::Off;
}

//==============================================================================
// Detection
//==============================================================================

bool LaunchkeyDriver::isLaunchkeyName (const juce::String& deviceName)
{
    return nameMatches (deviceName);
}

std::optional<DeviceSelection> LaunchkeyDriver::findDevice()
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

LaunchkeyDriver::LaunchkeyDriver()
{
    for (auto& state : padStates)
        state.store (static_cast<std::uint8_t> (PadState::idle));
}

LaunchkeyDriver::~LaunchkeyDriver()
{
    disconnect();
}

void LaunchkeyDriver::setListeners (DriverListeners newListeners)
{
    const std::lock_guard<std::mutex> lock (listenerMutex);
    listeners = std::move (newListeners);
}

bool LaunchkeyDriver::connect()
{
    const auto selection = findDevice();

    if (! selection.has_value())
    {
        setError ("no Launchkey Mini MK3 input/output pair found");
        return false;
    }

    return connect (*selection);
}

bool LaunchkeyDriver::connect (const DeviceSelection& selection)
{
    disconnect();

    if (selection.inputIdentifier.isEmpty() || selection.outputIdentifier.isEmpty())
    {
        setError ("the Launchkey device selection is incomplete");
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
        setError ("the session-mode handshake could not be sent");
        disconnect();
        return false;
    }

    clearError();
    return true;
}

void LaunchkeyDriver::disconnect()
{
    connected.store (false);

    if (input != nullptr)
    {
        input->stop();
        input.reset();
    }

    output.reset();
}

bool LaunchkeyDriver::isConnected() const noexcept
{
    return connected.load();
}

juce::String LaunchkeyDriver::getInputName() const
{
    return inputName;
}

juce::String LaunchkeyDriver::getOutputName() const
{
    return outputName;
}

juce::String LaunchkeyDriver::getLastError() const
{
    const std::lock_guard<std::mutex> lock (errorMutex);
    return lastError;
}

//==============================================================================
// Handshake
//==============================================================================

bool LaunchkeyDriver::sendHandshake()
{
    const bool sysexOk = sendSysEx (Launchkey::sessionModeFrame());
    const bool dawOk = sendChannelMessage (Launchkey::dawModeOn);
    const bool layoutOk = sendChannelMessage (Launchkey::sessionPadLayout);

    return sysexOk && dawOk && layoutOk;
}

bool LaunchkeyDriver::hasReceivedHandshakeEcho() const noexcept
{
    return handshakeEcho.load();
}

//==============================================================================
// LEDs
//==============================================================================

void LaunchkeyDriver::setPadState (int padIndex, PadState state)
{
    if (padIndex < Launchkey::padFirstIndex || padIndex > Launchkey::padLastIndex)
        return;

    padStates[static_cast<std::size_t> (padIndex)].store (
        static_cast<std::uint8_t> (state));

    sendPadColour (padIndex, colourForState (state));
}

PadState LaunchkeyDriver::getPadState (int padIndex) const noexcept
{
    if (padIndex < Launchkey::padFirstIndex || padIndex > Launchkey::padLastIndex)
        return PadState::idle;

    return static_cast<PadState> (
        padStates[static_cast<std::size_t> (padIndex)].load());
}

void LaunchkeyDriver::setAllPadStates (PadState state)
{
    for (int pad = 0; pad < Launchkey::padCount; ++pad)
        padStates[static_cast<std::size_t> (pad)].store (
            static_cast<std::uint8_t> (state));

    refreshLeds();
}

void LaunchkeyDriver::refreshLeds()
{
    for (int pad = 0; pad < Launchkey::padCount; ++pad)
        sendPadColour (pad, colourForState (getPadState (pad)));
}

bool LaunchkeyDriver::sendPadColour (int padIndex,
                                     Launchkey::Colour colour,
                                     Launchkey::LedMode mode)
{
    if (padIndex < Launchkey::padFirstIndex || padIndex > Launchkey::padLastIndex)
        return false;

    const auto bytes = Launchkey::padColourMessage (padIndex, colour, mode);
    return sendBytes (bytes.data(), static_cast<int> (bytes.size()));
}

bool LaunchkeyDriver::sendChannelMessage (const Launchkey::ChannelMessage& message)
{
    const std::array<std::uint8_t, 3> bytes {
        message.status, message.data1, message.data2
    };

    return sendBytes (bytes.data(), static_cast<int> (bytes.size()));
}

bool LaunchkeyDriver::sendSysEx (const Launchkey::SysExFrame& frame)
{
    if (! frame.isValid())
        return false;

    return sendBytes (frame.bytes.data(), static_cast<int> (frame.bytes.size()));
}

//==============================================================================
// Incoming MIDI
//==============================================================================

void LaunchkeyDriver::handleIncomingMidiMessage (juce::MidiInput*,
                                                 const juce::MidiMessage& message)
{
    if (message.isSysEx())
    {
        const auto bytes = std::span<const std::uint8_t> (
            message.getRawData(),
            static_cast<std::size_t> (message.getRawDataSize()));

        if (Launchkey::isSessionModeFrame (bytes))
            handshakeEcho.store (true);

        return;
    }

    if (message.isNoteOnOrOff())
    {
        const int note = message.getNoteNumber();
        const bool pressed = message.isNoteOn() && message.getVelocity() > 0;
        const float velocity = message.getFloatVelocity();

        if (const auto pad = Launchkey::padIndexForNote (note))
        {
            dispatchPad (*pad, pressed);
            return;
        }

        if (Launchkey::isKeyNote (note))
            dispatchKey (note, velocity, pressed);

        return;
    }

    if (message.isController()
        && message.getChannel() == Launchkey::encoderMidiChannel)
    {
        if (const auto encoder = Launchkey::encoderIndexForCC (message.getControllerNumber()))
            dispatchEncoder (*encoder, message.getControllerValue());
    }
}

void LaunchkeyDriver::dispatchPad (int padIndex, bool pressed)
{
    DriverListeners copy;

    {
        const std::lock_guard<std::mutex> lock (listenerMutex);
        copy = listeners;
    }

    if (copy.padEvent)
        copy.padEvent (padIndex, pressed);
}

void LaunchkeyDriver::dispatchEncoder (int encoderIndex, int value)
{
    DriverListeners copy;

    {
        const std::lock_guard<std::mutex> lock (listenerMutex);
        copy = listeners;
    }

    if (copy.encoderEvent)
        copy.encoderEvent (encoderIndex, value);
}

void LaunchkeyDriver::dispatchKey (int note, float velocity, bool pressed)
{
    DriverListeners copy;

    {
        const std::lock_guard<std::mutex> lock (listenerMutex);
        copy = listeners;
    }

    if (copy.keyEvent)
        copy.keyEvent (note, velocity, pressed);
}

//==============================================================================
// Low-level output
//==============================================================================

bool LaunchkeyDriver::sendBytes (const std::uint8_t* bytes, int size)
{
    if (bytes == nullptr || size <= 0)
        return false;

    const std::lock_guard<std::mutex> lock (sendMutex);

    if (output == nullptr)
        return false;

    output->sendMessageNow (juce::MidiMessage (bytes, size));
    return true;
}

void LaunchkeyDriver::setError (const juce::String& message)
{
    const std::lock_guard<std::mutex> lock (errorMutex);
    lastError = message;
}

void LaunchkeyDriver::clearError()
{
    const std::lock_guard<std::mutex> lock (errorMutex);
    lastError.clear();
}

} // namespace Controller
