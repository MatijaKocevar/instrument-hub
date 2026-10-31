#pragma once

#include "LaunchkeyProtocol.h"
#include "PadState.h"

#include <juce_audio_devices/juce_audio_devices.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>

namespace Controller
{

/** The LED colour used for a pad state.

    - idle    dark grey
    - warming orange
    - active  green
    - error   red
*/
Launchkey::Colour colourForState (PadState state) noexcept;

//==============================================================================
// Device discovery
//==============================================================================

/** The input/output pair selected for the controller. */
struct DeviceSelection
{
    juce::String inputIdentifier;
    juce::String outputIdentifier;
    juce::String inputName;
    juce::String outputName;
};

//==============================================================================
// Mapping callbacks
//==============================================================================

/**
    Everything the driver hands to the mapping layer.

    The driver owns no opinion about what a pad or encoder does. It decodes the
    protocol and calls these callbacks on the MIDI input thread; `set.yaml`
    mapping (`T043`) is what decides which slot a pad arms and where an encoder
    goes. Callbacks are copied before they are invoked, so replacing them while
    the input thread runs is safe.

    - `padEvent`     pad index 0..15 and whether the pad went down or up.
    - `encoderEvent` encoder index 0..7 and the raw CC value 0..127.
    - `keyEvent`     note number, velocity 0..1 and whether the key went down
                     or up. Pad-range notes are never reported here; the driver
                     forwards key notes to whatever the mapping has armed.
*/
struct DriverListeners
{
    std::function<void (int padIndex, bool pressed)> padEvent;
    std::function<void (int encoderIndex, int value)> encoderEvent;
    std::function<void (int note, float velocity, bool pressed)> keyEvent;
};

//==============================================================================
// Driver
//==============================================================================

/**
    Novation Launchkey Mini MK3 I/O on top of `juce::MidiInput`/`MidiOutput`.

    Responsibilities:

    - Detect the device by name (the simulator deliberately reports the same
      name as the hardware).
    - Open both directions and send the session-mode handshake on connect:
      the reserved SysEx frame followed by the documented DAW-mode note and pad
      layout CC, in exactly that order.
    - Decode pads (notes 36..51), encoders (CC 21..28 on channel 16) and key
      notes, then dispatch them to `DriverListeners`.
    - Write pad LEDs from `PadState` using the state colour table.
    - Mirror the session-mode SysEx when the device echoes it.

    Threading: `connect()`, `disconnect()`, `setListeners()`, `setPadState()`
    and `refreshLeds()` are control-thread calls. The incoming MIDI callback
    runs on JUCE's MIDI thread. No method here is called from the audio thread
    and the driver never touches the audio graph directly.
*/
class LaunchkeyDriver final : private juce::MidiInputCallback
{
public:
    LaunchkeyDriver();
    ~LaunchkeyDriver() override;

    //==============================================================================
    // Detection

    /** True when a JUCE device name belongs to the target controller. */
    static bool isLaunchkeyName (const juce::String& deviceName);

    /** The first Launchkey Mini MK3 input/output pair, if one is visible.

        A device whose name also contains "DAW" is preferred over any other
        matching port, because that is the port used for pads and LEDs.
    */
    static std::optional<DeviceSelection> findDevice();

    //==============================================================================
    // Connection

    /** Replaces the mapping callbacks. Safe while the driver is connected. */
    void setListeners (DriverListeners newListeners);

    /** Detects and connects to the controller, then sends the handshake. */
    bool connect();

    /** Connects to an explicitly selected pair, then sends the handshake. */
    bool connect (const DeviceSelection& selection);

    void disconnect();

    bool isConnected() const noexcept;

    juce::String getInputName() const;
    juce::String getOutputName() const;
    juce::String getLastError() const;

    //==============================================================================
    // Handshake

    /** Sends the documented session-mode sequence:

            F0 00 20 29 02 0F 0C 01 0D F7
            9F 0C 7F        (DAW mode on, channel 16, note 12)
            BF 03 02        (session pad layout)

        This is the exact three-message sequence in
        `tests/midi/fixtures/session-handshake.json`.
    */
    bool sendHandshake();

    /** True once the device has echoed the reserved session-mode SysEx. */
    bool hasReceivedHandshakeEcho() const noexcept;

    //==============================================================================
    // LEDs

    /** Stores a pad state and writes its LED immediately. Out-of-range pads are
        ignored. Safe to call from the MIDI input thread.
    */
    void setPadState (int padIndex, PadState state);

    PadState getPadState (int padIndex) const noexcept;

    /** Sets every pad to one state and refreshes all 16 LEDs. */
    void setAllPadStates (PadState state);

    /** Rewrites all 16 LEDs from the current states. */
    void refreshLeds();

    bool sendPadColour (int padIndex,
                        Launchkey::Colour colour,
                        Launchkey::LedMode mode = Launchkey::LedMode::Static);

    bool sendChannelMessage (const Launchkey::ChannelMessage& message);

    bool sendSysEx (const Launchkey::SysExFrame& frame);

private:
    //==============================================================================
    void handleIncomingMidiMessage (juce::MidiInput* source,
                                    const juce::MidiMessage& message) override;

    void dispatchPad (int padIndex, bool pressed);
    void dispatchEncoder (int encoderIndex, int value);
    void dispatchKey (int note, float velocity, bool pressed);

    bool sendBytes (const std::uint8_t* bytes, int size);

    void setError (const juce::String& message);
    void clearError();

    std::unique_ptr<juce::MidiInput> input;
    std::unique_ptr<juce::MidiOutput> output;

    juce::String inputName;
    juce::String outputName;
    juce::String lastError;

    mutable std::mutex sendMutex;
    mutable std::mutex listenerMutex;
    mutable std::mutex errorMutex;

    DriverListeners listeners;

    std::array<std::atomic<std::uint8_t>, Launchkey::padCount> padStates {};

    std::atomic<bool> connected { false };
    std::atomic<bool> handshakeEcho { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LaunchkeyDriver)
};

} // namespace Controller
