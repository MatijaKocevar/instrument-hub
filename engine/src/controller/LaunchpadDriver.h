#pragma once

#include "LaunchpadLed.h"
#include "LaunchpadProtocol.h"

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

class LaunchpadDriver final : private juce::MidiInputCallback
{
public:
    struct DeviceSelection
    {
        juce::String inputIdentifier;
        juce::String outputIdentifier;
        juce::String inputName;
        juce::String outputName;
    };

    struct DriverListeners
    {
        std::function<void (int padIndex, bool pressed)> padEvent;
        std::function<void (int cc, bool pressed)> buttonEvent;
    };

    LaunchpadDriver();
    ~LaunchpadDriver() override;

    static bool isLaunchpadName (const juce::String& deviceName);

    static std::optional<DeviceSelection> findDevice();

    void setListeners (DriverListeners newListeners);

    bool connect();

    bool connect (const DeviceSelection& selection);

    void disconnect();

    bool isConnected() const noexcept;

    bool canSendLeds() const noexcept;

    juce::String getInputName() const;
    juce::String getOutputName() const;
    juce::String getLastError() const;

    bool sendHandshake();

    bool hasReceivedHandshakeEcho() const noexcept;

    void setPadState (int padIndex, PadState state);

    PadState getPadState (int padIndex) const noexcept;

    void setPadLed (int padIndex, PatchLed led);

    PatchLed getPadLed (int padIndex) const noexcept;

    void setPatchLed (int padIndex,
                      int colorIndex,
                      bool armed,
                      bool warming,
                      bool error);

    void setAllPadStates (PadState state);

    void setButtonState (int cc, PadState state);

    PadState getButtonState (int cc) const noexcept;

    void setAllButtonStates (PadState state);

    static constexpr int controlButtonCount =
        Launchpad::topRowCount + Launchpad::rightColumnCount;

    void refreshLeds();

    void clearLeds();

    bool sendPadColour (int padIndex,
                        Launchpad::Colour colour,
                        Launchpad::LedMode mode = Launchpad::LedMode::Static);

    bool sendPadVelocity (int padIndex,
                          std::uint8_t velocity,
                          Launchpad::LedMode mode = Launchpad::LedMode::Static);

    bool sendButtonColour (int cc,
                           Launchpad::Colour colour,
                           Launchpad::LedMode mode = Launchpad::LedMode::Static);

    bool sendSysEx (const Launchpad::SysExFrame& frame);

    using MessageSink = std::function<void (const juce::MidiMessage&)>;

    void setMessageSink (MessageSink sink);

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
    std::array<std::atomic<std::uint16_t>, Launchpad::padCount> padLeds {};
    std::array<std::atomic<std::uint8_t>, controlButtonCount> buttonStates {};

    std::atomic<bool> connected { false };
    std::atomic<bool> handshakeEcho { false };

    mutable std::mutex sinkMutex;
    MessageSink messageSink;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LaunchpadDriver)
};

}
