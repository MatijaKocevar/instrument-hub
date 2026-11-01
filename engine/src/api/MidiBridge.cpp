#include "MidiBridge.h"

#include "controller/LaunchpadDriver.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace Api
{

namespace
{
std::string toStd (const juce::String& value)
{
    return value.toStdString();
}

bool containsIgnoringCase (const juce::String& name, const char* fragment)
{
    return name.toLowerCase().contains (fragment);
}

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
}

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

        state.enabled = false;
        state.open = false;
        inputs.push_back (std::move (state));
    }

    return inputs;
}

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

            if (state.launchpad.name.empty() || portForName (juce::String (input.name)) == "MI")
                state.launchpad.name = input.name;
        }
    }

    if (state.launchpad.name.empty() && driver == nullptr)
        state.launchpad.error = "no Launchpad Mini MK3 input found";

    state.launchpad.port = portForName (juce::String (state.launchpad.name));

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

    forwardMidiMessage (name, message);
}

}
