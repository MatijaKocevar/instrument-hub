#include "Rewind.h"

#include "Fixtures.h"

namespace AudioTest::ClockOut
{

void runRewind()
{
    std::cout << "\n-- rewind/reposition while playing --" << std::endl;

    AudioEngine::Transport transport;
    transport.prepare (48000.0, 128);
    transport.setBpm (120.0);
    transport.start();

    AudioEngine::ClockOut clock;
    render (clock, transport, 48000, 128);   // ticks 1..48

    transport.setPpq (10.0);                 // jump while running

    auto jumpBlock = render (clock, transport, 128, 128);
    check (jumpBlock.empty(),
           "repositioning does not emit a burst of skipped ticks");

    auto afterJump = render (clock, transport, 48000, 128);

    bool startsAt241 = ! afterJump.empty()
                       && afterJump.front().type == AudioEngine::ClockOut::Type::clock
                       && afterJump.front().tick == 241;

    check (startsAt241, "the generator resynchronises to tick 241 at PPQ 10");
    check (countClocks (afterJump) == 48,
           "one second after the jump emits 48 clocks at 120 BPM");
}

} // namespace AudioTest::ClockOut
