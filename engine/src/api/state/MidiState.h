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

    std::string port;

    bool enabled = false;
    bool open = false;
    std::uint64_t events = 0;
    std::string lastEvent;
};

struct MidiPortState
{
    std::string name;

    std::string port;

    bool connected = false;

    bool handshake = false;

    std::string inputName;
    std::string outputName;
    std::string error;
};

struct MidiPadTestState
{
    bool active = false;
    int lit = 0;
    int durationMs = 0;
};

struct MidiStateSnapshot
{
    std::vector<MidiInputState> inputs;
    MidiPortState launchpad;
    MidiPortState keystation;
    MidiPadTestState padTest;
    std::string error;
};

using MidiStateProvider = std::function<MidiStateSnapshot()>;

using MidiInputHandler = std::function<bool (const std::string& id,
                                             bool enabled,
                                             std::string& error)>;

using MidiPadTestHandler = std::function<bool (std::string& error)>;

}
