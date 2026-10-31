#pragma once

#include "Server.h"

#include <juce_audio_devices/juce_audio_devices.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Controller
{
class LaunchpadDriver;
}

namespace Api
{

/**
    MIDI device enumeration and the Launchpad pad test (T126).

    `GET /midi` reads the ALSA devices through JUCE, reports which inputs the
    user has enabled, and describes the two controllers the rig cares about:
    the Launchpad Mini MK3 (pads and LEDs on the MI port) and an optional
    Keystation keyboard.

    Enabling a generic input opens it through JUCE and counts its messages;
    the Launchpad is deliberately not opened here, because the driver owns its
    MI port. Enabling or disabling that row connects or disconnects the driver.

    `runPadTest()` lights every pad and animates a column for ~900 ms. It is
    the only code path that writes the grid from outside the LED policy; the
    `restore` callback repaints the real grid when the animation ends.

    Threading: HTTP threads call `snapshot()` and the handlers, the MIDI input
    callbacks update counters, and the pad-test thread writes LEDs. All shared
    state is guarded; nothing here is called from the audio callback.
*/
class MidiBridge final : private juce::MidiInputCallback
{
public:
    /** Duration of the pad-test animation, in milliseconds. */
    static constexpr int padTestDurationMs = 900;

    MidiBridge();
    ~MidiBridge() override;

    /** Attach the Launchpad driver owned by the application. Pass null when
        the driver is being destroyed. */
    void setLaunchpad (Controller::LaunchpadDriver* driver);

    /** Attach the consumer that enabled inputs play into. In the application
        this is `Session::pushMidi`. Pass an empty function to detach. */
    void setMidiSink (std::function<bool (const juce::MidiMessage&)> sink);

    /** Forward one short message from a named input to the session.

        Launchpad MI/DA ports are controller surfaces, not instruments: their
        notes arm patches and are never forwarded. Returns true only when the
        message went into the session queue. MIDI input threads call this
        through `handleIncomingMidiMessage`; tests call it directly. */
    bool forwardMidiMessage (const juce::String& deviceName,
                             const juce::MidiMessage& message);

    /** Called after the pad test clears the surface so the grid can repaint. */
    using RestoreHandler = std::function<void()>;
    void setRestoreHandler (RestoreHandler handler);

    /** The complete `GET /midi` payload. */
    MidiStateSnapshot snapshot();

    /** Enable or disable one input. Returns false with a reason. */
    bool setInputEnabled (const std::string& id, bool enabled, std::string& error);

    /** Light every pad and animate a column. Returns false when no Launchpad
        output is available. */
    bool runPadTest (std::string& error);

    bool isPadTestActive() const noexcept;

    //==========================================================================
    // Hardware-free helpers (shared with the tests)

    /** `launchpad`, `keystation` or `input`, from a JUCE/ALSA device name. */
    static std::string roleForName (const juce::String& name);

    /** Short port label (`MI`, `DA`, `USB`) from a device name. */
    static std::string portForName (const juce::String& name);

    /** Every MIDI input JUCE can see, with roles and default switches. */
    static std::vector<MidiInputState> enumerateInputs();

private:
    struct Monitor
    {
        std::unique_ptr<juce::MidiInput> input;
        std::uint64_t events = 0;
        std::string lastEvent;
    };

    void handleIncomingMidiMessage (juce::MidiInput* source,
                                    const juce::MidiMessage& message) override;

    void paintPadTestFrame (int frame);
    void runPadTestAnimation (std::uint64_t generation);
    void closeInputs();

    static std::string describeMessage (const juce::MidiMessage& message);

    std::atomic<Controller::LaunchpadDriver*> launchpad { nullptr };

    /** The session queue writer. Set once by the application/tests; guarded
        because MIDI callbacks and the control thread run concurrently. */
    mutable std::mutex sinkMutex;
    std::function<bool (const juce::MidiMessage&)> midiSink;

    /** Serialises the SPSC writer: several enabled inputs each run on their
        own MIDI thread. The audio-thread reader is still lock-free. */
    std::mutex forwardMutex;

    mutable std::mutex monitorMutex;
    std::map<std::string, Monitor> monitors;

    std::mutex padTestMutex;
    std::thread padTestThread;
    std::atomic<bool> padTestActive { false };
    std::atomic<bool> shuttingDown { false };
    std::atomic<int> padTestLit { 0 };
    std::atomic<std::uint64_t> padTestGeneration { 0 };

    mutable std::mutex restoreMutex;
    RestoreHandler restoreHandler;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiBridge)
};

} // namespace Api
