// `ihub-selftest --midi-probe`: hardware-free check of the virtual Launchkey.
//
// It plays the role of the application: one ALSA client subscribes to the
// simulator's source and writes LED feedback back to the simulator's
// destination. The probe asserts the portable things the driver depends on,
// not scheduler timing:
//
//   1. the port named `Launchkey Mini MK3 SIM` is visible to an enumerator;
//   2. a fixture of 16 pad presses arrives with notes 36..51 in order, each
//      followed by its note-off;
//   3. 16 LED messages written back are recorded in the order they were sent.

#include "MidiProbe.h"

#include "LaunchkeySim.h"

#include "controller/LaunchkeyProtocol.h"

#include <alsa/asoundlib.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifndef IHUB_MIDI_FIXTURE_DIR
#define IHUB_MIDI_FIXTURE_DIR "tests/midi/fixtures"
#endif

namespace LaunchkeySim
{
namespace
{
using Clock = std::chrono::steady_clock;

std::string defaultFixturePath()
{
    return std::string (IHUB_MIDI_FIXTURE_DIR) + "/pads-16.json";
}

std::string alsaMessage (const std::string& what, int error)
{
    return what + ": " + snd_strerror (error);
}

void checkAlsa (int error, const std::string& what)
{
    if (error < 0)
        throw std::runtime_error (alsaMessage (what, error));
}

struct ProbeSequencer
{
    snd_seq_t* seq = nullptr;
    int port = -1;

    ProbeSequencer()
    {
        checkAlsa (snd_seq_open (&seq, "default", SND_SEQ_OPEN_DUPLEX, 0),
                   "probe: cannot open ALSA sequencer");
        checkAlsa (snd_seq_set_client_name (seq, "ihub-selftest-midi-probe"),
                   "probe: cannot set client name");
        checkAlsa (snd_seq_nonblock (seq, 1),
                   "probe: cannot switch to non-blocking mode");

        port = snd_seq_create_simple_port (
            seq, "probe",
            SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_DUPLEX
                | SND_SEQ_PORT_CAP_SUBS_READ | SND_SEQ_PORT_CAP_SUBS_WRITE,
            SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);

        checkAlsa (port, "probe: cannot create the probe port");
        snd_seq_set_input_buffer_size (seq, 16384);
    }

    ~ProbeSequencer()
    {
        if (seq != nullptr)
            snd_seq_close (seq);
    }

    ProbeSequencer (const ProbeSequencer&) = delete;
    ProbeSequencer& operator= (const ProbeSequencer&) = delete;
};

struct Incoming
{
    int type = 0;
    int channel = 0;
    int a = 0;
    int b = 0;
    std::vector<std::uint8_t> bytes;
};

void receiveUntil (snd_seq_t* seq,
                   std::vector<Incoming>& out,
                   std::size_t wanted,
                   int timeoutMs)
{
    const auto deadline = Clock::now() + std::chrono::milliseconds (timeoutMs);

    while (out.size() < wanted && Clock::now() < deadline)
    {
        snd_seq_event_t* event = nullptr;
        const int result = snd_seq_event_input (seq, &event);

        if (result >= 0)
        {
            Incoming message;
            message.type = event->type;

            switch (event->type)
            {
                case SND_SEQ_EVENT_NOTEON:
                case SND_SEQ_EVENT_NOTEOFF:
                    message.channel = (event->data.note.channel & 0x0F) + 1;
                    message.a = event->data.note.note;
                    message.b = event->data.note.velocity;
                    break;

                case SND_SEQ_EVENT_CONTROLLER:
                    message.channel = (event->data.control.channel & 0x0F) + 1;
                    message.a = event->data.control.param;
                    message.b = event->data.control.value;
                    break;

                case SND_SEQ_EVENT_SYSEX:
                {
                    const auto* data = static_cast<const std::uint8_t*> (event->data.ext.ptr);
                    message.bytes.assign (data, data + event->data.ext.len);
                    break;
                }

                default:
                    continue;
            }

            out.push_back (std::move (message));
            continue;
        }

        if (result == -ENOSPC)
            break;

        if (result != -EAGAIN)
            throw std::runtime_error (alsaMessage ("probe: cannot read incoming MIDI", result));

        std::this_thread::sleep_for (std::chrono::milliseconds (1));
    }
}

void sendNote (snd_seq_t* seq,
               int port,
               int client,
               int destinationPort,
               int channel,
               int note,
               int velocity)
{
    snd_seq_event_t event;
    snd_seq_ev_clear (&event);
    snd_seq_ev_set_source (&event, port);
    snd_seq_ev_set_dest (&event, client, destinationPort);
    snd_seq_ev_set_direct (&event);
    snd_seq_ev_set_noteon (&event, channel, note, velocity);
    checkAlsa (snd_seq_event_output_direct (seq, &event),
               "probe: cannot send LED note");
}

bool checkPadSequence (const std::vector<Incoming>& messages,
                       std::vector<int>& noteOns,
                       std::vector<int>& noteOffs)
{
    noteOns.clear();
    noteOffs.clear();

    for (const auto& message : messages)
    {
        if (message.type == SND_SEQ_EVENT_NOTEON && Launchkey::isPadNote (message.a))
            noteOns.push_back (message.a);
        else if (message.type == SND_SEQ_EVENT_NOTEOFF && Launchkey::isPadNote (message.a))
            noteOffs.push_back (message.a);
    }

    std::vector<int> expectedOn;
    std::vector<int> expectedOff;

    for (int pad = 0; pad < Launchkey::padCount; ++pad)
    {
        expectedOn.push_back (Launchkey::padNoteForIndex (pad));
        expectedOff.push_back (Launchkey::padNoteForIndex (pad));
    }

    return noteOns == expectedOn && noteOffs == expectedOff;
}
} // namespace

int runMidiProbe (const std::string& fixturePath)
{
    bool passed = true;

    try
    {
        const std::string path = fixturePath.empty() ? defaultFixturePath() : fixturePath;
        std::cout << "midi: fixture=" << path << std::endl;

        Simulator simulator;
        simulator.open();

        // --- 1. The port is visible ---------------------------------------
        const auto ports = enumeratePorts();
        bool simListed = false;

        for (const auto& port : ports)
        {
            std::cout << "midi: port " << port.describe() << std::endl;

            if (isSimPort (port))
                simListed = true;
        }

        std::cout << "check: sim_port_listed=" << (simListed ? "ok" : "FAIL")
                  << " name='" << kPortName << "'" << std::endl;
        passed = passed && simListed;

        // --- 2. Fixture delivery ------------------------------------------
        ProbeSequencer probe;
        checkAlsa (snd_seq_connect_from (probe.seq, probe.port,
                                         simulator.clientId(), simulator.portId()),
                   "probe: cannot subscribe to the simulator source");
        checkAlsa (snd_seq_connect_to (probe.seq, probe.port,
                                       simulator.clientId(), simulator.portId()),
                   "probe: cannot subscribe the simulator destination");

        const Fixture fixture = loadFixture (path);

        if (fixture.events.empty())
            throw std::runtime_error ("fixture " + path + " contains no events");

        simulator.replay (fixture);

        const std::size_t expectedMessages = static_cast<std::size_t> (
            std::count_if (fixture.events.begin(), fixture.events.end(),
                           [] (const Event& event)
                           {
                               return event.type == "noteon" || event.type == "noteoff";
                           }));

        std::vector<Incoming> received;
        receiveUntil (probe.seq, received, expectedMessages, 2000);

        std::vector<int> noteOns;
        std::vector<int> noteOffs;
        const bool orderOk = checkPadSequence (received, noteOns, noteOffs);

        std::cout << "check: pad_presses=" << noteOns.size()
                  << " note_offs=" << noteOffs.size()
                  << " order=" << (orderOk ? "ok" : "FAIL") << std::endl;

        if (! orderOk)
        {
            std::cout << "check: received_noteons=";

            for (std::size_t i = 0; i < noteOns.size(); ++i)
                std::cout << (i == 0 ? "" : ",") << noteOns[i];

            std::cout << std::endl;
        }

        passed = passed && orderOk && noteOns.size() == 16;

        // --- 3. LED feedback is recorded ----------------------------------
        for (int pad = 0; pad < Launchkey::padCount; ++pad)
        {
            const auto message = Launchkey::padColourMessage (pad, Launchkey::Colour::Green);
            sendNote (probe.seq, probe.port,
                      simulator.clientId(), simulator.portId(),
                      message[0] & 0x0F, message[1], message[2]);
        }

        const auto deadline = Clock::now() + std::chrono::milliseconds (1000);

        while (simulator.recorded().size() < Launchkey::padCount && Clock::now() < deadline)
        {
            simulator.pump();
            std::this_thread::sleep_for (std::chrono::milliseconds (1));
        }

        simulator.pump();

        std::vector<int> recordedNotes;
        bool recordedColours = true;

        for (const auto& message : simulator.recorded())
        {
            if (message.type != "noteon" || message.channel != 1)
            {
                recordedColours = false;
                continue;
            }

            recordedNotes.push_back (message.data1);

            if (message.data2 != Launchkey::velocityFor (Launchkey::Colour::Green))
                recordedColours = false;
        }

        std::vector<int> expectedLedNotes;

        for (int pad = 0; pad < Launchkey::padCount; ++pad)
            expectedLedNotes.push_back (Launchkey::sessionLedNoteForIndex (pad));

        const bool recordingOk = recordedNotes == expectedLedNotes
                                 && recordedColours
                                 && simulator.recorded().size() == Launchkey::padCount;

        std::cout << "check: recorded_leds=" << simulator.recorded().size()
                  << " order=" << (recordedNotes == expectedLedNotes ? "ok" : "FAIL")
                  << " colours=" << (recordedColours ? "ok" : "FAIL") << std::endl;
        passed = passed && recordingOk;

        // --- 4. Recording serialisation -----------------------------------
        const auto recordingPath = std::filesystem::temp_directory_path()
                                      / "ihub-midi-probe-recording.jsonl";
        writeRecording (recordingPath.string(), simulator.recorded());

        std::size_t lines = 0;
        {
            std::ifstream file (recordingPath);

            for (std::string line; std::getline (file, line);)
                if (! line.empty())
                    ++lines;
        }

        std::error_code ignored;
        std::filesystem::remove (recordingPath, ignored);

        const bool fileOk = lines == simulator.recorded().size();
        std::cout << "check: recording_file_lines=" << lines
                  << " " << (fileOk ? "ok" : "FAIL") << std::endl;
        passed = passed && fileOk;

        std::cout << (passed ? "PASS" : "FAIL")
                  << ": ihub-selftest midi probe" << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL: " << e.what() << std::endl;
        return 1;
    }

    return passed ? 0 : 1;
}

} // namespace LaunchkeySim
