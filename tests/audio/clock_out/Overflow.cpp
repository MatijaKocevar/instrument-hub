#include "Overflow.h"

#include "Fixtures.h"

namespace AudioTest::ClockOut
{

void runOverflow()
{
    std::cout << "\n-- small output buffer --" << std::endl;

    AudioEngine::Transport transport;
    transport.prepare (48000.0, 9600);
    transport.setBpm (120.0);
    transport.start();

    AudioEngine::ClockOut clock;

    AudioEngine::ClockOut::Event roomy[256];
    const int firstCount = clock.generate (transport, 9600, roomy, 256);
    check (firstCount == 10, "a 9600-frame block at 120 BPM carries start and 9 clocks");

    transport.process (9600);

    const auto droppedBefore = clock.getDroppedEventCount();

    AudioEngine::ClockOut::Event tiny[1];
    const int written = clock.generate (transport, 9600, tiny, 1);

    check (written == 1, "generate never writes past the caller's capacity");
    check (clock.getDroppedEventCount() - droppedBefore == 9,
           "events that do not fit are counted as dropped");
    check (clock.getNextTick() == 20,
           "the tick counter advances past the skipped events");

    check (clock.generate (transport, 128, nullptr, 0) == 0,
           "a null/zero-capacity destination is ignored");
}

}
