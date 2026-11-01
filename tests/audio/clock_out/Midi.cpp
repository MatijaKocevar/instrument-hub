#include "Midi.h"

#include "Fixtures.h"

namespace AudioTest::ClockOut
{

void runMidi()
{
    std::cout << "\n-- output queue and MIDI bytes --" << std::endl;

    AudioEngine::ClockOut clock;

    check (static_cast<std::uint8_t> (AudioEngine::ClockOut::Type::clock) == 0xF8
               && static_cast<std::uint8_t> (AudioEngine::ClockOut::Type::start) == 0xFA
               && static_cast<std::uint8_t> (AudioEngine::ClockOut::Type::continue_) == 0xFB
               && static_cast<std::uint8_t> (AudioEngine::ClockOut::Type::stop) == 0xFC,
           "message values are the MIDI system real-time bytes");

    AudioEngine::Transport transport;
    transport.prepare (48000.0, 128);
    transport.setBpm (120.0);
    transport.start();

    // 1 s playing, then stop; drain everything through a sink.
    for (int block = 0; block < 375; ++block)
    {
        clock.process (transport, 128);
        transport.process (128);
    }

    transport.stop();
    clock.process (transport, 128);

    RecordingSink sink;
    const int sent = clock.drainTo (sink);

    check (sent == 50, "drainTo forwards start, 48 clocks and stop");
    check (! sink.bytes.empty() && sink.bytes.front() == 0xFA,
           "the first byte is start (0xFA)");
    check (! sink.bytes.empty() && sink.bytes.back() == 0xFC,
           "the last byte is stop (0xFC)");
    check (static_cast<int> (std::count (sink.bytes.begin(), sink.bytes.end(),
                                         static_cast<std::uint8_t> (0xF8))) == 48,
           "48 clock bytes (0xF8) were sent");
    check (clock.getPendingEventCount() == 0, "drainTo empties the queue");
    check (clock.drainTo (sink) == 0, "a second drain sends nothing");
}

} // namespace AudioTest::ClockOut
