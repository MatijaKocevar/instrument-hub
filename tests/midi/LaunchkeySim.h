#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

/**
    Virtual Novation Launchkey Mini MK3 on the ALSA sequencer.

    The simulator owns one duplex port named `Launchkey Mini MK3 SIM`. Fixture
    events are written into that port (the app subscribes and receives them),
    and every MIDI message the app writes back is recorded. Both directions
    use the same port so a host that enumerates ALSA sequencer ports sees one
    device named exactly like the hardware.

    Nothing here touches the audio path; the simulator is a test tool and may
    allocate or sleep freely.
*/
namespace LaunchkeySim
{

/** The ALSA client and port name. Also the name the driver matches on. */
inline constexpr const char* kPortName   = "Launchkey Mini MK3 SIM";
inline constexpr const char* kClientName = "Launchkey Mini MK3 SIM";

//==============================================================================
// Fixtures
//==============================================================================

/** One scripted event from a JSON fixture, as documented in plan/TESTING.md. */
struct Event
{
    std::int64_t t = 0; ///< milliseconds from the start of the fixture
    std::string type;   ///< noteon, noteoff, cc, sysex, programchange, pitchbend
    int channel = 1;    ///< 1..16
    int note = 0;       ///< 0..127
    int velocity = 0;   ///< 0..127
    int controller = 0; ///< 0..127
    int value = 0;      ///< CC value, program number or raw 14-bit pitch bend
    std::vector<std::uint8_t> bytes; ///< complete SysEx message for type == sysex
};

/** A parsed fixture. Events are replayed in timestamp order. */
struct Fixture
{
    std::vector<Event> events;
};

/** Loads and validates a fixture. Throws `std::runtime_error` on bad input. */
Fixture loadFixture (const std::string& path);

//==============================================================================
// Recording
//==============================================================================

/** One message received from the application. */
struct RecordedMessage
{
    std::int64_t t = 0; ///< milliseconds since the simulator was opened
    std::string type;   ///< noteon, noteoff, cc, programchange, pitchbend, sysex
    int channel = 0;    ///< 1..16, 0 for SysEx
    int data1 = 0;      ///< note, controller, program or pitch-bend LSB
    int data2 = 0;      ///< velocity, value or pitch-bend MSB
    std::vector<std::uint8_t> bytes; ///< the complete raw MIDI message

    /** One JSON object per message, matching the fixture vocabulary. */
    std::string toJson() const;
};

/** Writes one JSON object per line. Throws `std::runtime_error` on I/O failure. */
void writeRecording (const std::string& path,
                     const std::vector<RecordedMessage>& messages);

//==============================================================================
// Port discovery
//==============================================================================

/** A port visible on the ALSA sequencer. */
struct PortInfo
{
    int client = -1;
    int port = -1;
    std::string clientName;
    std::string portName;
    unsigned int capabilities = 0;
    bool source = false;      ///< can be read from (sends MIDI)
    bool destination = false; ///< can be written to (receives MIDI)

    std::string id() const;   ///< "client:port"
    std::string describe() const;
};

/** Enumerates every ALSA sequencer port currently visible. */
std::vector<PortInfo> enumeratePorts();

/** True when a port is the simulator's port (client or port name matches). */
bool isSimPort (const PortInfo& port);

//==============================================================================
// Simulator
//==============================================================================

class Simulator
{
public:
    Simulator();
    ~Simulator();

    Simulator (Simulator&&) noexcept;
    Simulator& operator= (Simulator&&) noexcept;

    Simulator (const Simulator&) = delete;
    Simulator& operator= (const Simulator&) = delete;

    /**
        Creates the ALSA client and the `Launchkey Mini MK3 SIM` duplex port.

        Throws `std::runtime_error` when ALSA is unavailable or the port cannot
        be created.
    */
    void open();

    bool isOpen() const noexcept;
    int clientId() const noexcept;
    int portId() const noexcept;

    /** Writes a fixture into the port, honouring its millisecond timestamps. */
    void replay (const Fixture& fixture, double speed = 1.0);

    /** Reads pending messages from the application and appends them. */
    std::size_t pump();

    /** Pumps for up to `timeoutMs` and returns everything recorded so far. */
    const std::vector<RecordedMessage>& drain (int timeoutMs);

    const std::vector<RecordedMessage>& recorded() const noexcept;
    void clearRecording() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace LaunchkeySim
