#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace Api
{

struct MidiInputState
{
    static constexpr int gridPadCount = 64;

    std::string id;
    std::string name;
    std::string role = "keyboard";

    std::string port;

    bool enabled = false;
    bool open = false;
    bool handshake = false;
    bool leds = false;

    int noteBase = -1;
    int learnedNotes = 0;
    bool learnActive = false;

    std::uint64_t events = 0;
    std::string lastEvent;
    std::string error;
};

struct MidiStateSnapshot
{
    std::vector<MidiInputState> inputs;
    std::string error;
};

struct MidiInputPatch
{
    std::string id;

    bool hasEnabled = false;
    bool enabled = false;

    bool hasRole = false;
    std::string role;

    bool hasNoteBase = false;
    int noteBase = -1;

    bool hasLearn = false;
    bool learn = false;
};

using MidiStateProvider = std::function<MidiStateSnapshot()>;

using MidiInputHandler = std::function<bool (const MidiInputPatch& patch,
                                             std::string& error)>;

}
