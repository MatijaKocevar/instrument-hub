#pragma once

#include "LaunchpadProtocol.h"
#include "PadState.h"

#include <juce_audio_devices/juce_audio_devices.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>

namespace Controller
{

/** The LED colour used for a pad state on the Launchpad. */
Launchpad::Colour launchpadColourForState (PadState state) noexcept;

/**
    Novation Launchpad Mini MK3 I/O on top of `juce::MidiInput`/`MidiOutput`.

    The device exposes two ports. On hardware, pads report and LEDs accept data
    on `LPMiniMK3 MI`; the `DA` port is inert for channel messages. The driver
    therefore prefers the MI port instead of the DAW port.

    Responsibilities:

    - Detect the device by name and open the MI port in both directions.
    - Send the programmer-mode handshake on connect: programmer mode, external
      LED feedback, wake from LED sleep.
    - Decode the 8x8 grid (notes 11..88) and the top-row/right-column CCs.
    - Write pad LEDs from `PadState`.

    Threading: `connect()`, `disconnect()`, `setListeners()`, `setPadState()`
    and `refreshLeds()` are control-thread calls. The incoming MIDI callback
    runs on JUCE's MIDI thread. Nothing here is called from the audio thread.
*/
class LaunchpadDriver final : private juce::MidiInputCallback
{
public:
    /** The input/output pair selected for the controller. */
    struct DeviceSelection
    {
        juce::String inputIdentifier;
        juce::String outputIdentifier;
        juce::String inputName;
        juce::String outputName;
    };

    /**
        Everything the driver hands to the mapping layer.

        - `padEvent`    grid pad 0..63 and whether it went down or up.
        - `buttonEvent` a top-row/right-column/logo CC and whether it went down
                        or up. `cc` is the raw controller number.
    */
    struct DriverListeners
    {
        std::function<void (int padIndex, bool pressed)> padEvent;
        std::function<void (int cc, bool pressed)> buttonEvent;
    };

    LaunchpadDriver();
    ~LaunchpadDriver() override;

    //==============================================================================
    // Detection

    /** True when a JUCE device name belongs to the target controller. */
    static bool isLaunchpadName (const juce::String& deviceName);

    /** The first Launchpad Mini MK3 input/output pair, if one is visible. */
    static std::optional<DeviceSelection> findDevice();

    //==============================================================================
    // Connection

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

    /** Sends programmer mode, external LED feedback and wake-from-sleep. */
    bool sendHandshake();

    /** True once the device has echoed the programmer-mode frame. */
    bool hasReceivedHandshakeEcho() const noexcept;

    //==============================================================================
    // LEDs

    void setPadState (int padIndex, PadState state);

    PadState getPadState (int padIndex) const noexcept;

    void setAllPadStates (PadState state);

    /** Sets the LED of a top-row or right-column button. `cc` is the raw
        controller number (91..98 or 89, 79, ... 19). */
    void setButtonState (int cc, PadState state);

    PadState getButtonState (int cc) const noexcept;

    void setAllButtonStates (PadState state);

    /** Number of addressable non-grid buttons (8 top row + 8 right column). */
    static constexpr int controlButtonCount =
        Launchpad::topRowCount + Launchpad::rightColumnCount;

    /** Rewrites every grid and control-row LED from the current states. */
    void refreshLeds();

    /** Turns every grid and control-row LED off. */
    void clearLeds();

    bool sendPadColour (int padIndex,
                        Launchpad::Colour colour,
                        Launchpad::LedMode mode = Launchpad::LedMode::Static);

    bool sendButtonColour (int cc,
                           Launchpad::Colour colour,
                           Launchpad::LedMode mode = Launchpad::LedMode::Static);

    bool sendSysEx (const Launchpad::SysExFrame& frame);

    //==============================================================================
    // Scripted (hardware-free) operation

    /** Receives every MIDI message the driver emits. A scripted test can
        capture the LED stream without opening a MIDI port. */
    using MessageSink = std::function<void (const juce::MidiMessage&)>;

    /** Installs or clears the output tap. It runs on the calling thread. */
    void setMessageSink (MessageSink sink);

    /** Feeds one incoming message through the same decoder as the MIDI thread.

        This is the seam for scripted tests and simulators: no device needs to
        be connected. The driver still knows nothing about audio; it only turns
        notes and CCs into pad/button events and LED messages.
    */
    void injectMidiMessage (const juce::MidiMessage& message);

private:
    void handleIncomingMidiMessage (juce::MidiInput* source,
                                    const juce::MidiMessage& message) override;

    void dispatchPad (int padIndex, bool pressed);
    void dispatchButton (int cc, bool pressed);

    static int controlButtonSlot (int cc) noexcept;

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

    std::array<std::atomic<std::uint8_t>, Launchpad::padCount> padStates {};
    std::array<std::atomic<std::uint8_t>, controlButtonCount> buttonStates {};

    std::atomic<bool> connected { false };
    std::atomic<bool> handshakeEcho { false };

    mutable std::mutex sinkMutex;
    MessageSink messageSink;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LaunchpadDriver)
};

} // namespace Controller
