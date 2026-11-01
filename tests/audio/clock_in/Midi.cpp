#include "Midi.h"

#include "Fixtures.h"

namespace AudioTest::ClockIn
{

void runMidi()
{
    std::cout << "\n-- MIDI byte values --" << std::endl;

    check (static_cast<std::uint8_t> (AudioEngine::ClockIn::Type::clock) == 0xF8
               && static_cast<std::uint8_t> (AudioEngine::ClockIn::Type::start) == 0xFA
               && static_cast<std::uint8_t> (AudioEngine::ClockIn::Type::continue_) == 0xFB
               && static_cast<std::uint8_t> (AudioEngine::ClockIn::Type::stop) == 0xFC,
           "the message values are the MIDI system real-time bytes");
    check (AudioEngine::ClockIn::pulsesPerQuarterNote == 24,
           "the follower counts 24 pulses per quarter note");
}

} // namespace AudioTest::ClockIn
