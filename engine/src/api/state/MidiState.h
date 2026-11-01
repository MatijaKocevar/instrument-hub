#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace Api
{

struct MidiInputState
{
    std::string id;
    std::string name;
    std::string role = "input";

    /** Short port label (`MI`, `DA`, `USB`) when the device name carries one. */
    std::string port;

    bool enabled = false;
    bool open = false;
    std::uint64_t events = 0;
    std::string lastEvent;
};

/** Status of one named controller port (Launchpad MI, Keystation USB). */
struct MidiPortState
{
    /** Device name, empty when the device is not present. */
    std::string name;

    /** Short port label: `MI`, `DA` or `USB`. */
    std::string port;

    bool connected = false;

    /** True once the controller echoed its programmer/session-mode frame. */
    bool handshake = false;

    std::string inputName;
    std::string outputName;
    std::string error;
};

/** Result of the most recent pad test. */
struct MidiPadTestState
{
    bool active = false;
    int lit = 0;
    int durationMs = 0;
};

/** Everything `GET /midi` returns. */
struct MidiStateSnapshot
{
    std::vector<MidiInputState> inputs;
    MidiPortState launchpad;
    MidiPortState keystation;
    MidiPadTestState padTest;
    std::string error;
};

/** Supplies the current MIDI device and controller state. Called on an HTTP
    thread; a live implementation must not block the audio callback. */
using MidiStateProvider = std::function<MidiStateSnapshot()>;

/** Enables or disables one MIDI input. Returns false with a message when the
    port cannot be opened. */
using MidiInputHandler = std::function<bool (const std::string& id,
                                             bool enabled,
                                             std::string& error)>;

/** Runs the Launchpad pad test. Returns false with a message when there is no
    connected Launchpad. */
using MidiPadTestHandler = std::function<bool (std::string& error)>;

} // namespace Api
