#include "MidiBridge.h"

#include "controller/LaunchpadDriver.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <utility>

namespace Api
{

namespace
{
constexpr const char* launchpadPadTestError =
    "the Launchpad is not connected; check that its MI port is visible";

std::string toStd (const juce::String& value)
{
    return value.toStdString();
}

bool containsIgnoringCase (const juce::String& name, const char* fragment)
{
    return name.toLowerCase().contains (fragment);
}

/** Last whitespace-separated token of a device name, upper-cased. */
std::string lastToken (const juce::String& name)
{
    const auto text = name.trim().toStdString();
    const auto space = text.find_last_of (' ');

    std::string token = space == std::string::npos ? text : text.substr (space + 1);

    std::transform (token.begin(), token.end(), token.begin(), [] (unsigned char character)
    {
        return static_cast<char> (std::toupper (character));
    });

    return token;
}
} // namespace

//==============================================================================
MidiBridge::MidiBridge() = default;

MidiBridge::~MidiBridge()
{
    shuttingDown.store (true, std::memory_order_release);
    padTestActive.store (false, std::memory_order_release);
    padTestGeneration.fetch_add (1, std::memory_order_acq_rel);

    {
        const std::lock_guard<std::mutex> lock (padTestMutex);

        if (padTestThread.joinable())
            padTestThread.join();
    }

    launchpad.store (nullptr, std::memory_order_release);
    setMidiSink (nullptr);
    closeInputs();
}

void MidiBridge::setLaunchpad (Controller::LaunchpadDriver* driver)
{
    launchpad.store (driver, std::memory_order_release);
}

void MidiBridge::setMidiSink (std::function<bool (const juce::MidiMessage&)> sink)
{
    const std::lock_guard<std::mutex> lock (sinkMutex);
    midiSink = std::move (sink);
}

bool MidiBridge::forwardMidiMessage (const juce::String& deviceName,
                                     const juce::MidiMessage& message)
{
    // The Launchpad's MI port carries pad notes as controller events. They
    // arm a patch through the driver/mapping path and must never reach an
    // instrument as a playable note.
    if (roleForName (deviceName) == "launchpad")
        return false;

    const std::lock_guard<std::mutex> sinkLock (sinkMutex);

    if (! midiSink)
        return false;

    const std::lock_guard<std::mutex> writeLock (forwardMutex);
    return midiSink (message);
}

void MidiBridge::setRestoreHandler (RestoreHandler handler)
{
    const std::lock_guard<std::mutex> lock (restoreMutex);
    restoreHandler = std::move (handler);
}

//==============================================================================
std::string MidiBridge::roleForName (const juce::String& name)
{
    if (Controller::LaunchpadDriver::isLaunchpadName (name))
        return "launchpad";

    if (containsIgnoringCase (name, "keystation"))
        return "keystation";

    return "input";
}

std::string MidiBridge::portForName (const juce::String& name)
{
    if (containsIgnoringCase (name, "keystation"))
        return "USB";

    const auto token = lastToken (name);

    if (token == "MI" || token == "DA")
        return token;

    if (containsIgnoringCase (name, "usb"))
        return "USB";

    return {};
}

std::vector<MidiInputState> MidiBridge::enumerateInputs()
{
    std::vector<MidiInputState> inputs;

    for (const auto& device : juce::MidiInput::getAvailableDevices())
    {
        MidiInputState state;
        state.id = toStd (device.identifier);
        state.name = toStd (device.name);
        state.role = roleForName (device.name);
        state.port = portForName (device.name);

        // The Launchpad driver opens the MI port at startup; every other
        // input starts switched off so the engine never steals a port.
        state.enabled = false;
        state.open = false;
        inputs.push_back (std::move (state));
    }

    return inputs;
}

//==============================================================================
MidiStateSnapshot MidiBridge::snapshot()
{
    MidiStateSnapshot state;
    state.inputs = enumerateInputs();

    auto* driver = launchpad.load (std::memory_order_acquire);

    {
        const std::lock_guard<std::mutex> lock (monitorMutex);

        for (auto& input : state.inputs)
        {
            if (input.role == "launchpad")
            {
                // Only the port the driver actually owns is switched on; the
                // DA row stays visible and off because it carries no grid data.
                const bool driverOpen = driver != nullptr && driver->isConnected();
                const bool ownedPort = driverOpen
                                       && (input.port == "MI"
                                           || input.name == toStd (driver->getInputName()));
                input.enabled = ownedPort;
                input.open = ownedPort;
                continue;
            }

            const auto monitor = monitors.find (input.id);

            if (monitor == monitors.end())
                continue;

            input.enabled = true;
            input.open = monitor->second.input != nullptr;
            input.events = monitor->second.events;
            input.lastEvent = monitor->second.lastEvent;
        }
    }

    // Launchpad status: the driver is authoritative when it is attached.
    if (driver != nullptr)
    {
        state.launchpad.connected = driver->isConnected();
        state.launchpad.handshake = driver->hasReceivedHandshakeEcho();
        state.launchpad.inputName = toStd (driver->getInputName());
        state.launchpad.outputName = toStd (driver->getOutputName());
        state.launchpad.error = toStd (driver->getLastError());
        state.launchpad.name = state.launchpad.inputName;
    }

    if (state.launchpad.name.empty())
    {
        for (const auto& input : state.inputs)
        {
            if (input.role != "launchpad")
                continue;

            // Prefer the MI port when both ports are visible.
            if (state.launchpad.name.empty() || portForName (juce::String (input.name)) == "MI")
                state.launchpad.name = input.name;
        }
    }

    if (state.launchpad.name.empty() && driver == nullptr)
        state.launchpad.error = "no Launchpad Mini MK3 input found";

    state.launchpad.port = portForName (juce::String (state.launchpad.name));

    // Keystation status: present in the ALSA list, owned by the bridge when
    // its checkbox is on.
    for (const auto& input : state.inputs)
    {
        if (input.role != "keystation")
            continue;

        state.keystation.name = input.name;
        state.keystation.inputName = input.open ? input.name : "";
        state.keystation.connected = input.open;
        state.keystation.port = portForName (juce::String (input.name));
        break;
    }

    if (state.keystation.name.empty())
        state.keystation.error = "no Keystation found on the MIDI inputs";

    state.padTest.active = padTestActive.load (std::memory_order_acquire);
    state.padTest.lit = padTestLit.load (std::memory_order_acquire);
    state.padTest.durationMs = padTestDurationMs;

    return state;
}

//==============================================================================
bool MidiBridge::setInputEnabled (const std::string& id, bool enabled, std::string& error)
{
    const auto inputs = enumerateInputs();
    const auto device = std::find_if (inputs.begin(), inputs.end(),
                                      [&id] (const MidiInputState& input)
                                      {
                                          return input.id == id;
                                      });

    if (device == inputs.end())
    {
        error = "unknown MIDI input: " + id;
        return false;
    }

    if (device->role == "launchpad")
    {
        auto* driver = launchpad.load (std::memory_order_acquire);

        if (driver == nullptr)
        {
            error = "the Launchpad driver is not attached";
            return false;
        }

        if (device->port == "DA")
        {
            error = "the Launchpad grid uses the MI port; the DA port carries no pads or LEDs";
            return false;
        }

        if (enabled == driver->isConnected())
            return true;

        if (enabled)
        {
            if (! driver->connect())
            {
                error = toStd (driver->getLastError());

                if (error.empty())
                    error = "could not connect the Launchpad";

                return false;
            }
        }
        else
        {
            driver->disconnect();
        }

        return true;
    }

    if (enabled)
    {
        {
            const std::lock_guard<std::mutex> lock (monitorMutex);
            const auto existing = monitors.find (id);

            if (existing != monitors.end() && existing->second.input != nullptr)
                return true;
        }

        auto input = juce::MidiInput::openDevice (juce::String (id), this);

        if (input == nullptr)
        {
            error = "cannot open MIDI input '" + device->name + "'";
            return false;
        }

        input->start();

        const std::lock_guard<std::mutex> lock (monitorMutex);
        auto& monitor = monitors[id];
        monitor.input = std::move (input);
        return true;
    }

    std::unique_ptr<juce::MidiInput> closing;

    {
        const std::lock_guard<std::mutex> lock (monitorMutex);
        const auto existing = monitors.find (id);

        if (existing == monitors.end())
            return true;

        closing = std::move (existing->second.input);
        monitors.erase (existing);
    }

    if (closing != nullptr)
        closing->stop();

    return true;
}

//==============================================================================
bool MidiBridge::isPadTestActive() const noexcept
{
    return padTestActive.load (std::memory_order_acquire);
}

bool MidiBridge::runPadTest (std::string& error)
{
    auto* driver = launchpad.load (std::memory_order_acquire);

    if (driver == nullptr)
    {
        error = "the Launchpad driver is not attached";
        return false;
    }

    if (! driver->isConnected() && ! driver->canSendLeds())
    {
        error = toStd (driver->getLastError());

        if (error.empty())
            error = launchpadPadTestError;

        return false;
    }

    const std::lock_guard<std::mutex> lock (padTestMutex);

    if (padTestThread.joinable())
        padTestThread.join();

    if (shuttingDown.load (std::memory_order_acquire))
        return false;

    const auto generation = padTestGeneration.fetch_add (1, std::memory_order_acq_rel) + 1;
    padTestLit.store (Launchpad::padCount, std::memory_order_release);
    padTestActive.store (true, std::memory_order_release);

    paintPadTestFrame (0);

    padTestThread = std::thread ([this, generation]
    {
        runPadTestAnimation (generation);
    });

    return true;
}

void MidiBridge::paintPadTestFrame (int frame)
{
    auto* driver = launchpad.load (std::memory_order_acquire);

    if (driver == nullptr)
        return;

    for (int pad = 0; pad < Launchpad::padCount; ++pad)
    {
        const int column = pad % 8;
        const bool armed = column == frame % 8;

        // Every pad shows its palette colour; a bright red column walks
        // across the 8x8 grid so the test is visible on the hardware.
        driver->setPadLed (pad, Controller::launchpadLedForPatch (column, armed, false, false));
    }
}

void MidiBridge::runPadTestAnimation (std::uint64_t generation)
{
    for (int frame = 1; frame < 8; ++frame)
    {
        if (shuttingDown.load (std::memory_order_acquire)
            || padTestGeneration.load (std::memory_order_acquire) != generation)
            return;

        std::this_thread::sleep_for (std::chrono::milliseconds (padTestDurationMs / 8));

        if (shuttingDown.load (std::memory_order_acquire)
            || padTestGeneration.load (std::memory_order_acquire) != generation)
            return;

        paintPadTestFrame (frame);
    }

    // Clear the surface, then let the grid repaint itself.
    if (auto* driver = launchpad.load (std::memory_order_acquire);
        driver != nullptr && padTestGeneration.load (std::memory_order_acquire) == generation)
    {
        for (int pad = 0; pad < Launchpad::padCount; ++pad)
            driver->setPadLed (pad, {});
    }

    padTestLit.store (0, std::memory_order_release);
    padTestActive.store (false, std::memory_order_release);

    if (shuttingDown.load (std::memory_order_acquire))
        return;

    RestoreHandler restore;

    {
        const std::lock_guard<std::mutex> lock (restoreMutex);
        restore = restoreHandler;
    }

    if (restore)
        restore();
}

//==============================================================================
void MidiBridge::closeInputs()
{
    std::map<std::string, Monitor> closing;

    {
        const std::lock_guard<std::mutex> lock (monitorMutex);
        closing.swap (monitors);
    }

    for (auto& entry : closing)
    {
        if (entry.second.input != nullptr)
            entry.second.input->stop();
    }
}

std::string MidiBridge::describeMessage (const juce::MidiMessage& message)
{
    if (message.isNoteOn())
        return "note on " + std::to_string (message.getNoteNumber())
               + " vel " + std::to_string (message.getVelocity());

    if (message.isNoteOff())
        return "note off " + std::to_string (message.getNoteNumber());

    if (message.isController())
        return "CC " + std::to_string (message.getControllerNumber())
               + " = " + std::to_string (message.getControllerValue());

    if (message.isPitchWheel())
        return "pitch " + std::to_string (message.getPitchWheelValue());

    if (message.isAftertouch())
        return "aftertouch " + std::to_string (message.getNoteNumber());

    if (message.isSysEx())
        return "SysEx " + std::to_string (message.getRawDataSize()) + " bytes";

    return "message";
}

void MidiBridge::handleIncomingMidiMessage (juce::MidiInput* source,
                                            const juce::MidiMessage& message)
{
    if (source == nullptr)
        return;

    const auto id = toStd (source->getIdentifier());
    const auto name = source->getName();

    {
        const std::lock_guard<std::mutex> lock (monitorMutex);
        const auto monitor = monitors.find (id);

        if (monitor == monitors.end())
            return;

        monitor->second.events += 1;
        monitor->second.lastEvent = describeMessage (message);
    }

    // A monitor is opened only for a non-Launchpad input the user enabled in
    // the MIDI page. Forward it into the session's lock-free queue; the audio
    // callback drains it into the armed patch's block.
    forwardMidiMessage (name, message);
}

} // namespace Api
